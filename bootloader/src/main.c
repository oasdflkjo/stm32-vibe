#include <stdint.h>
#include "app_image.h"
#include "app_validation.h"
#include "boot/boot_state.h"
#include "boot/update_handoff.h"
#include "boot_policy.h"
#include "boot/boot_state_store.h"
#include "fault/fault.h"
#include "hal/itm.h"
#include "hal/uart.h"
#include "trace/trace.h"
#include "update_command.h"
#include "stm32l1xx.h"

__attribute__((naked, noreturn))
static void start_app(uint32_t stack_pointer, uint32_t reset_handler)
{
    (void)stack_pointer;
    (void)reset_handler;
    __asm volatile(
        "msr msp, r0\n"
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
                        uint32_t reset_handler)
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
    start_app(stack_pointer, reset_handler);
}

static void run_update_loop_forever(boot_update_loop_t *update_loop)
{
    while (1) {
        (void)boot_update_loop_poll(update_loop);
        if (update_loop->session.reset_requested != 0U) {
            (void)uart_drain_tx();
            NVIC_SystemReset();
        }
    }
}

static void enter_update_mode(void)
{
    boot_update_loop_t update_loop;

    if (uart_init(BOOT_UPDATE_UART_BAUD) == UART_RESULT_OK) {
        TRACE("BOOT update uart ready baud=%u", BOOT_UPDATE_UART_BAUD);
        boot_update_loop_init(&update_loop);
        run_update_loop_forever(&update_loop);
    }
    TRACE("BOOT update uart init failed");
    while (1) {
    }
}

static void probe_update_mode(void)
{
    boot_update_loop_t update_loop;

    if (uart_init(BOOT_UPDATE_UART_BAUD) != UART_RESULT_OK) {
        return;
    }

    boot_update_loop_init(&update_loop);
    for (uint32_t poll = 0U; poll < BOOT_UPDATE_PROBE_POLLS; poll++) {
        boot_update_poll_result_t result = boot_update_loop_poll(&update_loop);
        if ((result.bytes_received != 0U) || (result.packets_received != 0U) ||
            (result.parse_errors != 0U)) {
            TRACE("BOOT update probe hit bytes=%u packets=%u",
                  result.bytes_received, result.packets_received);
            run_update_loop_forever(&update_loop);
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

    if (!app_vectors_are_valid_for_slot(stack_pointer, reset_handler,
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
    jump_to_app(candidate.image_base, stack_pointer, reset_handler);
}
