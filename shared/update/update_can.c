#include "update/update_can.h"

uint32_t update_can_request_id(uint8_t node_id)
{
    return UPDATE_CAN_REQUEST_BASE_ID + node_id;
}

uint32_t update_can_response_id(uint8_t node_id)
{
    return UPDATE_CAN_RESPONSE_BASE_ID + node_id;
}

void update_can_reassembly_init(update_can_reassembly_t *reassembly)
{
    if (reassembly != 0) {
        *reassembly = (update_can_reassembly_t){0};
    }
}

static update_can_reassembly_result_t fail(update_can_reassembly_t *reassembly)
{
    reassembly->active = 0U;
    return UPDATE_CAN_REASSEMBLY_ERROR;
}

update_can_reassembly_result_t update_can_reassembly_feed(
    update_can_reassembly_t *reassembly,
    const can_frame_t *frame,
    uint32_t expected_can_id,
    const uint8_t **packet,
    size_t *packet_len)
{
    uint8_t control;
    uint8_t index;
    size_t data_offset;
    size_t chunk_len;

    if ((reassembly == 0) || (frame == 0) || (packet == 0) ||
        (packet_len == 0) || (frame->dlc == 0U) ||
        (frame->dlc > CAN_MAX_DATA_LEN)) {
        return UPDATE_CAN_REASSEMBLY_ERROR;
    }
    *packet = 0;
    *packet_len = 0U;
    if (frame->id != expected_can_id) {
        return UPDATE_CAN_REASSEMBLY_NONE;
    }

    control = frame->data[0];
    index = control & UPDATE_CAN_FRAGMENT_INDEX_MASK;
    if ((control & UPDATE_CAN_FRAGMENT_START) != 0U) {
        if ((index != 0U) || (frame->dlc < 3U)) {
            return fail(reassembly);
        }
        reassembly->expected_len = (uint16_t)frame->data[1] |
                                   ((uint16_t)frame->data[2] << 8U);
        if ((reassembly->expected_len < UPDATE_PROTOCOL_HEADER_SIZE +
                                         UPDATE_PROTOCOL_CRC_SIZE) ||
            (reassembly->expected_len > UPDATE_PROTOCOL_MAX_PACKET_SIZE)) {
            return fail(reassembly);
        }
        reassembly->received_len = 0U;
        reassembly->next_index = 1U;
        reassembly->active = 1U;
        data_offset = 3U;
    } else {
        if ((reassembly->active == 0U) ||
            (index != reassembly->next_index)) {
            return fail(reassembly);
        }
        reassembly->next_index++;
        data_offset = 1U;
    }

    chunk_len = frame->dlc - data_offset;
    if (chunk_len > (size_t)(reassembly->expected_len -
                             reassembly->received_len)) {
        size_t useful_len = reassembly->expected_len -
                            reassembly->received_len;
        if ((control & UPDATE_CAN_FRAGMENT_END) == 0U) {
            return fail(reassembly);
        }
        for (size_t byte = useful_len; byte < chunk_len; byte++) {
            if (frame->data[data_offset + byte] != 0U) {
                return fail(reassembly);
            }
        }
        chunk_len = useful_len;
    }
    for (size_t byte = 0U; byte < chunk_len; byte++) {
        reassembly->packet[reassembly->received_len + byte] =
            frame->data[data_offset + byte];
    }
    reassembly->received_len += (uint16_t)chunk_len;

    if ((control & UPDATE_CAN_FRAGMENT_END) != 0U) {
        if (reassembly->received_len != reassembly->expected_len) {
            return fail(reassembly);
        }
        reassembly->active = 0U;
        *packet = reassembly->packet;
        *packet_len = reassembly->received_len;
        return UPDATE_CAN_REASSEMBLY_COMPLETE;
    }
    if (reassembly->received_len == reassembly->expected_len) {
        return fail(reassembly);
    }
    return UPDATE_CAN_REASSEMBLY_NONE;
}

int update_can_fragmenter_init(update_can_fragmenter_t *fragmenter,
                               uint32_t can_id,
                               const uint8_t *packet,
                               size_t packet_len)
{
    if ((fragmenter == 0) || (packet == 0) ||
        (packet_len < UPDATE_PROTOCOL_HEADER_SIZE + UPDATE_PROTOCOL_CRC_SIZE) ||
        (packet_len > UPDATE_PROTOCOL_MAX_PACKET_SIZE) ||
        (can_id > 0x7FFU)) {
        return 0;
    }
    *fragmenter = (update_can_fragmenter_t){
        .packet = packet,
        .packet_len = packet_len,
        .can_id = can_id,
    };
    return 1;
}

int update_can_fragmenter_next(update_can_fragmenter_t *fragmenter,
                               can_frame_t *frame)
{
    size_t header_len;
    size_t capacity;
    size_t remaining;
    size_t chunk_len;
    uint8_t control;

    if ((fragmenter == 0) || (frame == 0) ||
        (fragmenter->offset >= fragmenter->packet_len)) {
        return 0;
    }

    control = fragmenter->fragment_index & UPDATE_CAN_FRAGMENT_INDEX_MASK;
    header_len = fragmenter->offset == 0U ? 3U : 1U;
    if (fragmenter->offset == 0U) {
        control |= UPDATE_CAN_FRAGMENT_START;
    }
    capacity = CAN_MAX_DATA_LEN - header_len;
    remaining = fragmenter->packet_len - fragmenter->offset;
    chunk_len = remaining < capacity ? remaining : capacity;
    if (chunk_len == remaining) {
        control |= UPDATE_CAN_FRAGMENT_END;
    }

    *frame = (can_frame_t){
        .id = fragmenter->can_id,
        .dlc = CAN_MAX_DATA_LEN,
    };
    frame->data[0] = control;
    if (header_len == 3U) {
        frame->data[1] = (uint8_t)fragmenter->packet_len;
        frame->data[2] = (uint8_t)(fragmenter->packet_len >> 8U);
    }
    for (size_t byte = 0U; byte < chunk_len; byte++) {
        frame->data[header_len + byte] =
            fragmenter->packet[fragmenter->offset + byte];
    }
    fragmenter->offset += chunk_len;
    fragmenter->fragment_index++;
    return 1;
}
