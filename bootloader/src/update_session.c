#include "update_session.h"

#include "app_image.h"
#include "app_validation.h"
#include "boot_flash.h"
#include "boot_policy.h"
#include "boot/boot_state_store.h"
#include "image/flash_layout.h"

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static int command_requires_active_session(uint8_t command)
{
    return (command == UPDATE_CMD_BLOCK) || (command == UPDATE_CMD_END) ||
           (command == UPDATE_CMD_VALIDATE) ||
           (command == UPDATE_CMD_ACTIVATE) ||
           (command == UPDATE_CMD_ABORT);
}

static update_status_t handle_begin(boot_update_session_t *session,
                                    const update_packet_t *packet)
{
    boot_state_record_t state;
    uint32_t image_size;
    uint32_t target_slot;

    if (packet->payload_len != 8U) {
        return UPDATE_STATUS_BAD_LENGTH;
    }
    image_size = read_u32_le(&packet->payload[0]);
    if ((image_size == 0U) || (image_size > APP_SLOT_SIZE)) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    boot_state_store_load(&state);
    target_slot = boot_policy_inactive_slot(&state);
    if (boot_flash_erase_slot(target_slot, image_size) != BOOT_FLASH_OK) {
        return UPDATE_STATUS_FLASH_ERROR;
    }

    session->session_id = packet->session_id;
    session->expected_image_size = image_size;
    session->expected_image_crc32 = read_u32_le(&packet->payload[4]);
    session->target_slot = target_slot;
    session->received_image_size = 0U;
    session->candidate_version = 0U;
    session->candidate_crc32 = 0U;
    session->session_active = 1U;
    session->transfer_complete = 0U;
    session->candidate_valid = 0U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_block(boot_update_session_t *session,
                                    const update_packet_t *packet)
{
    const uint8_t *target_base;

    if ((packet->payload_len == 0U) ||
        (packet->sequence > session->expected_image_size) ||
        (packet->payload_len >
         (session->expected_image_size - packet->sequence))) {
        return UPDATE_STATUS_BAD_SEQUENCE;
    }

    if (packet->sequence < session->received_image_size) {
        if (packet->payload_len >
            (session->received_image_size - packet->sequence)) {
            return UPDATE_STATUS_BAD_SEQUENCE;
        }
        target_base = boot_flash_slot_base(session->target_slot);
        if (target_base == 0) {
            return UPDATE_STATUS_BAD_STATE;
        }
        for (uint32_t index = 0U; index < packet->payload_len; index++) {
            if (target_base[packet->sequence + index] !=
                packet->payload[index]) {
                return UPDATE_STATUS_BAD_SEQUENCE;
            }
        }
        return UPDATE_STATUS_OK;
    }

    if (packet->sequence != session->received_image_size) {
        return UPDATE_STATUS_BAD_SEQUENCE;
    }
    if (boot_flash_write_slot(session->target_slot, packet->sequence,
                              packet->payload,
                              packet->payload_len) != BOOT_FLASH_OK) {
        return UPDATE_STATUS_FLASH_ERROR;
    }
    session->received_image_size += packet->payload_len;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_validate(boot_update_session_t *session)
{
    app_image_result_t image_result;
    const uint8_t *target_base = boot_flash_slot_base(session->target_slot);
    const uint32_t *vectors = (const uint32_t *)target_base;
    uint32_t slot_start = session->target_slot == BOOT_SLOT_A
                              ? APP_SLOT_A_START_ADDR : APP_SLOT_B_START_ADDR;
    uint32_t slot_end = session->target_slot == BOOT_SLOT_A
                            ? APP_SLOT_A_END_ADDR : APP_SLOT_B_END_ADDR;

    if (session->transfer_complete == 0U) {
        return UPDATE_STATUS_BAD_STATE;
    }
    if (target_base == 0) {
        return UPDATE_STATUS_BAD_STATE;
    }
    image_result = app_image_validate(target_base, APP_SLOT_SIZE);
    if ((image_result.status != APP_IMAGE_VALID) ||
        (image_result.image_size != session->expected_image_size) ||
        (image_result.expected_crc32 != session->expected_image_crc32) ||
        !app_vectors_are_valid_for_slot(vectors[0], vectors[1], slot_start,
                                        slot_end)) {
        return UPDATE_STATUS_BAD_IMAGE;
    }
    session->candidate_version = image_result.version;
    session->candidate_crc32 = image_result.expected_crc32;
    session->candidate_valid = 1U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_activate(boot_update_session_t *session)
{
    boot_state_record_t state;

    if (session->candidate_valid == 0U) {
        return UPDATE_STATUS_BAD_STATE;
    }
    boot_state_store_load(&state);
    boot_policy_mark_slot_pending(&state, session->target_slot,
                                  session->candidate_version,
                                  session->candidate_crc32);
    if (!boot_state_store_save_next(&state)) {
        return UPDATE_STATUS_FLASH_ERROR;
    }
    session->reset_requested = 1U;
    return UPDATE_STATUS_OK;
}

void boot_update_session_init(boot_update_session_t *session)
{
    if (session != 0) {
        *session = (boot_update_session_t){.target_slot = BOOT_SLOT_B};
    }
}

update_status_t boot_update_session_process(boot_update_session_t *session,
                                            const update_packet_t *packet)
{
    if ((session == 0) || (packet == 0)) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }
    if (command_requires_active_session(packet->command) &&
        ((session->session_active == 0U) ||
         (packet->session_id != session->session_id))) {
        return UPDATE_STATUS_BAD_STATE;
    }

    switch (packet->command) {
    case UPDATE_CMD_DISCOVER:
    case UPDATE_CMD_STATUS:
    case UPDATE_CMD_ENTER_UPDATE:
        return UPDATE_STATUS_OK;
    case UPDATE_CMD_BEGIN:
        return handle_begin(session, packet);
    case UPDATE_CMD_BLOCK:
        return handle_block(session, packet);
    case UPDATE_CMD_END:
        if (session->received_image_size != session->expected_image_size) {
            return UPDATE_STATUS_BAD_LENGTH;
        }
        session->transfer_complete = 1U;
        return UPDATE_STATUS_OK;
    case UPDATE_CMD_VALIDATE:
        return handle_validate(session);
    case UPDATE_CMD_ACTIVATE:
        return handle_activate(session);
    case UPDATE_CMD_ABORT:
        boot_update_session_init(session);
        return UPDATE_STATUS_OK;
    default:
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }
}
