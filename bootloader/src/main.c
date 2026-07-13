#include <stdint.h>
#include "app_image.h"
#include "app_validation.h"
#include "boot/boot_state.h"
#include "boot/boot_runtime.h"
#include "boot/update_handoff.h"
#include "boot_policy.h"
#include "boot/boot_state_store.h"
#include "fault/fault.h"
#include "hal/itm.h"
#include "hal/uart.h"
#include "hal/watchdog.h"
#include "trace/trace.h"
#include "update_command.h"
#ifdef BOARD_HAS_CAN
#include "update_can_loop.h"
#endif
#include "platform/device.h"

__attribute__((naked, noreturn))
static void start_app(uint32_t stack_pointer, uint32_t reset_handler,
                      uint32_t got_base)
{
    (void)stack_pointer;
    (void)reset_handler;
    (void)got_base;
    __asm volatile(
        "msr msp, r0\n"
        "mov r9, r2\n"
        "movs r0, #0\n"
        "msr control, r0\n"
        "msr basepri, r0\n"
        "msr faultmask, r0\n"
        "cpsie i\n"
        "dsb\n"
        "isb\n"
        "bx r1\n"
    );
}

__attribute__((noreturn))
static void jump_to_app(uint32_t vector_base,
                        uint32_t stack_pointer,
                        uint32_t reset_handler,
                        uint32_t got_base)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (uint32_t index = 0U;
         index < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));
         index++) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }

    SCB->VTOR = vector_base;
    __DSB();
    __ISB();
    start_app(stack_pointer, reset_handler, got_base);
}

static int prepare_relocatable_runtime(uint32_t image_base,
                                       const app_manifest_t *manifest)
{
    return boot_runtime_relocate(
        (const uint8_t *)image_base, image_base, manifest->image_size,
        app_manifest_vector_words(manifest), app_manifest_got_offset(manifest),
        app_manifest_got_size(manifest),
        (uint32_t *)BOOT_RUNTIME_VECTOR_ADDR, BOOT_RUNTIME_VECTOR_WORDS,
        (uint32_t *)BOOT_RUNTIME_GOT_ADDR, BOOT_RUNTIME_GOT_WORDS);
}

static void run_update_loop_forever(
    boot_update_loop_t *update_loop
#ifdef BOARD_HAS_CAN
    , boot_can_update_loop_t *can_loop
#endif
)
{
    while (1) {
        watchdog_refresh();
        (void)boot_update_loop_poll(update_loop);
#ifdef BOARD_HAS_CAN
        (void)boot_can_update_loop_poll(can_loop, update_loop);
#endif
        watchdog_refresh();
        if (update_loop->session.reset_requested != 0U) {
            (void)uart_drain_tx();
#ifdef BOARD_HAS_CAN
            (void)can_drain_tx();
#endif
            NVIC_SystemReset();
        }
    }
}

static void enter_update_mode(void)
{
    boot_update_loop_t update_loop;
    int uart_ready;
#ifdef BOARD_HAS_CAN
    boot_can_update_loop_t can_loop = {0};
    int can_ready;
#endif

    boot_update_loop_init(&update_loop);
    watchdog_refresh();
    uart_ready = uart_init(BOOT_UPDATE_UART_BAUD) == UART_RESULT_OK;
#ifdef BOARD_HAS_CAN
    can_ready = boot_can_update_loop_init(&can_loop, UPDATE_CAN_NODE_ID,
                                          500000U);
#endif
    if (uart_ready
#ifdef BOARD_HAS_CAN
        || can_ready
#endif
    ) {
        if (uart_ready) {
            TRACE("BOOT update uart ready baud=%u", BOOT_UPDATE_UART_BAUD);
        }
#ifdef BOARD_HAS_CAN
        if (can_ready) {
            TRACE("BOOT update can ready node=%u", UPDATE_CAN_NODE_ID);
        }
#endif
        run_update_loop_forever(
            &update_loop
#ifdef BOARD_HAS_CAN
            , &can_loop
#endif
        );
    }
    TRACE("BOOT update transport init failed");
    while (1) {
    }
}

static void prepare_boot_handoff(const boot_candidate_t *candidate,
                                 const app_image_result_t *image,
                                 const boot_state_record_t *state)
{
    boot_handoff_t handoff = {
        .magic = BOOT_HANDOFF_MAGIC,
        .version = BOOT_HANDOFF_VERSION,
        .size = sizeof(boot_handoff_t),
        .image_base = candidate->image_base,
        .image_size = image->image_size,
        .slot = candidate->slot,
        .state = candidate->boot_pending != 0U ? BOOT_HANDOFF_PENDING
                                               : BOOT_HANDOFF_CONFIRMED,
        .boot_attempt = candidate->boot_pending != 0U
                            ? state->pending_attempts
                            : 0U,
    };

    boot_handoff_update_crc(&handoff);
    *boot_handoff_shared() = handoff;
}

static void probe_update_mode(void)
{
    boot_update_loop_t update_loop;
    int uart_ready;
#ifdef BOARD_HAS_CAN
    boot_can_update_loop_t can_loop = {0};
    int can_ready;
#endif

    uart_ready = uart_init(BOOT_UPDATE_UART_BAUD) == UART_RESULT_OK;
#ifdef BOARD_HAS_CAN
    can_ready = boot_can_update_loop_init(&can_loop, UPDATE_CAN_NODE_ID,
                                          500000U);
#endif
    if (!uart_ready
#ifdef BOARD_HAS_CAN
        && !can_ready
#endif
    ) {
        return;
    }

    boot_update_loop_init(&update_loop);
    for (uint32_t poll = 0U; poll < BOOT_UPDATE_PROBE_POLLS; poll++) {
        watchdog_refresh();
        boot_update_poll_result_t result = boot_update_loop_poll(&update_loop);
        uint32_t can_frames = 0U;
#ifdef BOARD_HAS_CAN
        if (can_ready) {
            can_frames = boot_can_update_loop_poll(&can_loop, &update_loop);
        }
#endif
        if ((result.bytes_received != 0U) || (result.packets_received != 0U) ||
            (result.parse_errors != 0U) || (can_frames != 0U)) {
            TRACE("BOOT update probe hit bytes=%u packets=%u",
                  result.bytes_received, result.packets_received);
            run_update_loop_forever(
                &update_loop
#ifdef BOARD_HAS_CAN
                , &can_loop
#endif
            );
        }
    }
}

int main(void)
{
    uint32_t reset_cause = RCC->CSR;
    app_image_result_t image_result;
    boot_state_record_t boot_state;
    boot_candidate_t candidate;
    uint32_t boot_state_generation;
    const uint32_t *app_vectors;
    uint32_t stack_pointer;
    uint32_t reset_handler;
    const app_manifest_t *manifest;

    (void)watchdog_init(WATCHDOG_TIMEOUT_MS);
    itm_init(SystemCoreClock, TRACE_SWO_BAUD);
    fault_handlers_init();
    TRACE("BOOT reset csr=%08X", reset_cause);
    if ((reset_cause & RCC_CSR_IWDGRSTF) != 0U) {
        TRACE("BOOT reset cause=watchdog");
    }
    if (update_handoff_take()) {
        TRACE("BOOT update handoff");
        RCC->CSR |= RCC_CSR_RMVF;
        enter_update_mode();
    }
    if ((reset_cause & RCC_CSR_SFTRSTF) != 0U) {
        TRACE("BOOT reset cause=software update");
    }
    RCC->CSR |= RCC_CSR_RMVF;
    TRACE("BOOT start");
    probe_update_mode();

    boot_state_store_load(&boot_state);
    boot_state_generation = boot_state.generation;
    candidate = boot_policy_select_candidate(&boot_state);
    if (boot_state.generation != boot_state_generation) {
        (void)boot_state_store_save_next(&boot_state);
    }
    TRACE("BOOT state active=%u pending=%u gen=%u",
          boot_state.active_slot, boot_state.pending_slot,
          boot_state.generation);
    TRACE("BOOT state attempts=%u slot_a=%u slot_b=%u",
          boot_state.pending_attempts, boot_state.slot_a_status,
          boot_state.slot_b_status);

    TRACE("BOOT candidate slot=%u base=%08X pending=%u",
          candidate.slot, candidate.image_base, candidate.boot_pending);

    image_result = app_image_validate((const uint8_t *)candidate.image_base,
                                      candidate.image_end -
                                          candidate.image_base);
    if (image_result.status != APP_IMAGE_VALID) {
        TRACE("BOOT invalid image reason=%u", image_result.status);
        TRACE("BOOT image size=%u version=%u",
              image_result.image_size, image_result.version);
        TRACE("BOOT image crc expected=%08X actual=%08X",
              image_result.expected_crc32, image_result.calculated_crc32);
        if (candidate.boot_pending != 0U) {
            boot_policy_mark_pending_bad(&boot_state);
            (void)boot_state_store_save_next(&boot_state);
            candidate = boot_policy_select_candidate(&boot_state);
            image_result =
                app_image_validate((const uint8_t *)candidate.image_base,
                                   candidate.image_end - candidate.image_base);
            if (image_result.status != APP_IMAGE_VALID) {
                enter_update_mode();
            }
        } else {
            enter_update_mode();
        }
    }

    app_vectors = (const uint32_t *)candidate.image_base;
    stack_pointer = app_vectors[0];
    reset_handler = app_vectors[1];
    TRACE("BOOT image version=%u size=%u",
          image_result.version, image_result.image_size);

    manifest = (const app_manifest_t *)(candidate.image_base +
                                        APP_MANIFEST_OFFSET);
    if (app_manifest_is_relocatable(manifest)
            ? !app_relative_vectors_are_valid(stack_pointer, reset_handler,
                                               image_result.image_size)
            : !app_vectors_are_valid_for_slot(stack_pointer, reset_handler,
                                               candidate.image_base,
                                               candidate.image_end)) {
        TRACE("BOOT invalid app sp=%08X reset=%08X",
              stack_pointer, reset_handler);
        if (candidate.boot_pending != 0U) {
            boot_policy_mark_pending_bad(&boot_state);
            (void)boot_state_store_save_next(&boot_state);
        }
        enter_update_mode();
    }

    TRACE("BOOT jump app");
    prepare_boot_handoff(&candidate, &image_result, &boot_state);
    if (app_manifest_is_relocatable(manifest)) {
        if (!prepare_relocatable_runtime(candidate.image_base, manifest)) {
            TRACE("BOOT runtime relocation failed");
            enter_update_mode();
        }
        app_vectors = (const uint32_t *)BOOT_RUNTIME_VECTOR_ADDR;
        stack_pointer = app_vectors[0];
        reset_handler = app_vectors[1];
        jump_to_app(BOOT_RUNTIME_VECTOR_ADDR, stack_pointer, reset_handler,
                    BOOT_RUNTIME_GOT_ADDR);
    }
    jump_to_app(candidate.image_base, stack_pointer, reset_handler, 0U);
}
