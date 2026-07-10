# Application Platform Improvement Roadmap

## Purpose

Turn the current firmware repository into a small, predictable baseline on
which application developers can create products without copying platform
boilerplate or needing to understand bootloader internals.

This roadmap is intentionally incremental. Each phase should leave the current
NUCLEO-L152RE UART workflow working and tested.

## Current Scope

### In scope now

- A clear application lifecycle and platform API.
- Platform-owned startup, watchdog, fault handling, update servicing, and boot
  confirmation.
- A transport-independent firmware-update state machine.
- UART as the current update transport.
- Build-system and configuration cleanup.
- Host tests for platform contracts, rollback, and update behavior.
- Documentation and a minimal application template.

### Deferred until CAN hardware is available

- Production `can.c` and detailed CAN HAL design.
- STM32F4 bxCAN register implementation.
- CAN bit timing, filters, bus-off recovery, and hardware-in-loop tests.
- CAN packet fragmentation, addressing, and discovery details that depend on
  the selected bus/product requirements.

The existing `can.h`, mock, and tests may remain placeholders. Avoid expanding
them merely to simulate hardware we cannot validate.

## Embedded C Engineering Rules

Use the NASA “Power of Ten” guidance in `nasa.md` as risk-driven engineering
guidance, accounting for this platform's scope and testability needs:

- Keep per-poll and per-command work statically bounded. Treat reaching an
  artificial processing bound as an observable condition where it matters.
- Permit intentional infinite loops only for top-level firmware service/failure
  loops; inner processing must yield or terminate.
- Do not use heap allocation in target firmware.
- Keep functions and modules small enough to review as one coherent unit.
- Validate public-function parameters and all untrusted protocol/image data.
- Check fallible return values or explicitly document why they are ignored.
- Keep state and helper functions at the narrowest useful scope.
- Keep preprocessing simple and minimize build variants.
- Avoid pointer complexity. Use function pointers only at narrow, immutable
  testability/port boundaries where they reduce coupling.
- Compile with warnings as errors and add static analysis to CI incrementally.

These rules do not require replacing clear bounded code with less maintainable
simulations merely to satisfy a mechanical metric. Deviations should be small,
intentional, and reviewable.

## Target Developer Experience

Creating an application should require only:

1. Creating `apps/<name>/` from a small template.
2. Implementing application lifecycle callbacks.
3. Declaring application sources, version, and optional platform components.
4. Running `make APP=<name>` and `make test APP=<name>`.

Application code should not directly:

- Include an STM32 device header.
- Initialize faults, tracing, the watchdog, or the update transport.
- Refresh the hardware watchdog.
- Know boot-state record layout or slot addresses.
- Implement reset handoff to the bootloader.
- Select UART versus a future CAN update transport.
- Copy the complete `vibe` Makefile.

## Proposed Boundaries

```text
apps/<name>/
    Product behavior and application lifecycle callbacks

platform/
    Owns main(), startup order, event loop, watchdog supervision,
    boot confirmation, reset requests, update service, and diagnostics

services/update/
    Transport-independent update session and command state machine

transports/uart_update/
    UART initialization, byte-stream framing, packet send/receive

transports/can_update/                 (later)
    CAN fragmentation/reassembly and packet send/receive

bsp/<board>/
    Board resources, clocks, pins, peripheral selection, and memory layout

drivers/<mcu-family>/
    MCU peripheral implementations

bootloader/
    Image verification, A/B policy, persistent state, flash writer, and
    minimal recovery/update runtime
```

The precise directory names can change during implementation. The dependency
direction is the important part: applications depend on platform contracts;
platform code selects board and driver implementations.

## Phase 0 — Freeze and Document Existing Behavior

Goal: establish a safe refactoring baseline before moving responsibilities.

- [ ] Add a CI/test command that builds the bootloader and the single canonical
      relocatable application image, not only host tests.
- [ ] Record current flash sizes and fail the build if bootloader or app images
      exceed their assigned regions.
- [ ] Add an explicit support matrix to the README:
      `nucleo-l152re = supported`, `nucleo-f446re = planned`.
- [ ] Mark CAN interfaces as placeholders in their documentation.
- [ ] Document the current boot/update sequence from reset through activation
      and rollback.
- [ ] Keep `make test` passing throughout all later phases.

Acceptance criteria:

- CI builds both application slot variants and runs all current tests.
- Documentation no longer implies that STM32F4 or CAN is currently supported.
- The current UART update can still be performed using the documented command.

## Phase 1 — Define the Application Contract

Goal: make the platform, rather than each application, own system lifecycle.

- [x] Define a small public application interface, initially something like:

  ```c
  typedef enum {
      APP_INIT_OK,
      APP_INIT_FAILED,
  } app_init_result_t;

  app_init_result_t app_init(void);
  void app_process(uint32_t now_ms);
  bool app_is_healthy(void);
  ```

- [x] Add platform-owned `main()` that initializes diagnostics, timekeeping,
      update service, watchdog supervision, and then the application.
- [x] Move the current LED example behind these callbacks.
- [x] Add a platform reset service so applications never call CMSIS
      reset functions or write the update-handoff word directly.
- [x] Remove `#include "stm32l1xx.h"` from application sources.
- [ ] Define which callbacks may block. Prefer that `app_process()` does not
      block and returns promptly.
- [ ] Define failure behavior when `app_init()` fails or application health is
      lost.
- [ ] Add host tests for initialization order, failed initialization, normal
      processing, and reset requests.

Acceptance criteria:

- `apps/vibe` contains no MCU-specific include and no platform startup code.
- A minimal test application can be added without copying `main.c`.
- Platform lifecycle behavior is tested independently of `vibe`.

## Phase 2 — Add Time and Cooperative Scheduling

Goal: replace delay loops and ad hoc idle callbacks with predictable,
non-blocking application execution.

- [ ] Expose monotonic time through `platform_now_ms()` or a timer service.
- [x] Change the LED task to store its next deadline instead of calling
      `systick_delay_ms()` 500 times.
- [ ] Run platform services and `app_process(now)` from one small cooperative
      event loop.
- [ ] Specify the maximum allowed processing time for an application callback.
- [x] Add tests using explicit timestamps; tests do not depend on real delays.
- [ ] Add an idle hook suitable for a future low-power wait instruction.

Acceptance criteria:

- No application task uses `systick_delay_ms()`.
- Update servicing latency is bounded by the documented application callback
  budget.
- LED timing tests advance a fake clock and run deterministically.

## Phase 2A — Use One Relocatable Application Image

Goal: build one canonical application binary whose stored bytes are identical
whether it is installed in slot A or slot B.

Required invariants:

- The release contains one application binary and one metadata sidecar.
- The updater writes that binary unchanged to either inactive slot.
- CRC and later signature verification cover the same bytes in both slots.
- The bootloader supplies runtime slot/base information; the application image
  does not contain a build-time slot selection.
- Interrupt dispatch remains correct from either slot.

Tasks:

- [ ] Verify the selected ARM GCC position-independent code model on Cortex-M3,
      including references to code, constants, globals, and function calls.
- [ ] Define a slot-independent vector-table representation. Prefer storing
      handler offsets and constructing a relocated vector table in reserved RAM
      before application launch.
- [ ] Reserve and assert sufficient aligned RAM for the relocated vector table
      and boot handoff structure.
- [ ] Define a versioned, CRC-protected boot handoff containing at least:
      running slot, image base, image size, pending/confirmed state, and boot
      attempt number.
- [ ] Make the bootloader validate vector offsets, relocate vectors into RAM,
      set `VTOR`, and enter the relocated reset handler.
- [ ] Expose immutable boot information through a platform API; application
      code must not inspect boot-state flash or infer its slot from addresses.
- [ ] Change the linker script and finalizer to produce one canonical image
      without `APP_SLOT` or an absolute slot origin.
- [ ] Ensure startup code, `.data` initialization, constructors, and interrupt
      handlers operate from both physical slots.
- [ ] Update updater and flash-image tooling to reuse the exact same binary for
      slot A and slot B.
- [ ] Remove `APP_SLOT`, `firmware-slot-b`, `flash-slot-b`, and slot-specific
      release artifacts.
- [ ] Add host tests for handoff validation, vector relocation, invalid offsets,
      and both slot bases.
- [ ] Add a build assertion/test proving the bytes installed in A and B are
      identical.
- [ ] Hardware-test boot, UART discovery, SysTick interrupts, LED behavior,
      update activation, confirmation, and rollback from both slots.

Acceptance criteria:

- One finalized binary boots unchanged from slot A and slot B.
- No application build argument selects a slot.
- The platform reports the correct current slot from bootloader-provided data.
- No vector, code, constant, or data reference depends on a fixed flash slot.
- Updating A to B and B to A uses the same release artifact.

## Phase 3 — Fix Watchdog and Boot Confirmation Ownership

Goal: make rollback reflect real application health and prevent applications
from accidentally defeating the watchdog.

- [x] Add a platform watchdog supervisor; only it may call
      `watchdog_refresh()`.
- [x] Define initial required health inputs: application health plus successful
      event-loop progress. Extend this as more critical services are added.
- [x] Define the initial confirmation policy: application health must remain
      continuously true for the configured stabilization interval.
- [x] Add a platform-to-boot-state confirmation mechanism usable by the running
      application.
- [x] Reject remote `UPDATE_CMD_CONFIRM`; only local platform health may
      confirm a pending image.
- [x] Ensure a pending app that crashes, hangs, or fails initialization is not
      confirmed.
- [ ] Define behavior for watchdog reset while a pending image is running.
- [x] Clear and mark an exhausted pending image bad instead of leaving ambiguous
      persistent state.
- [ ] Add end-to-end host tests for successful confirmation, failed health,
      watchdog reset, exhausted attempts, and rollback.

Acceptance criteria:

- The updater cannot remotely declare an application healthy.
- A healthy pending application confirms itself through a platform API.
- An unhealthy pending application rolls back after the configured attempts.
- Application code cannot directly refresh the hardware watchdog.

## Phase 4 — Separate Update Protocol, Session, and Transport

Goal: retain UART today while allowing future CAN glue to replace it at build
time and reuse the complete update state machine. UART and CAN are not required
to operate simultaneously, so no runtime-polymorphic transport interface is
needed.

Create three distinct responsibilities:

1. **Protocol codec** — encodes and decodes logical update packets.
2. **Update session engine** — processes commands and owns transfer state,
   sequence rules, image validation, activation, and status responses.
3. **Transport-specific glue** — converts UART bytes, or later CAN frames, into
   complete logical packets and sends complete responses. Only one transport is
   selected into a firmware build.

Tasks:

- [x] Extract command handling from `bootloader/src/update_command.c` into a
      transport-independent session engine.
- [x] Keep `update_stream_t` exclusively in the UART-facing loop; it is a
      byte-stream framing concern.
- [x] Make the UART bootloader loop pass decoded packets to the independent
      session engine. Replace this glue at build time when CAN is implemented.
- [ ] When CAN arrives, replace the application UART update agent with CAN
      update-intent glue; do not add simultaneous transports unless a product
      requirement appears.
- [x] Store the session ID accepted by `BEGIN` and reject later commands from a
      different session.
- [x] Accept retries of already-written blocks only when their bytes match
      flash; reject changed or partially overlapping duplicates.
- [ ] Define session timeout, abort, and restart behavior.
- [ ] Make status/discovery responses carry useful structured data rather than
      only a two-byte ACK.
- [ ] Ensure no transport can invoke confirmation as a substitute for
      application health.
- [x] Add direct session-engine tests with no UART dependency.
- [ ] Retain separate UART-stream tests for resynchronization, partial packets,
      and corrupt frames.

Acceptance criteria:

- The update session engine has no includes or symbols containing `uart` or
  `can`.
- UART update behavior and tooling remain functional.
- Session behavior is covered directly without transport mocks.
- A future CAN implementation replaces UART glue at build time and needs only
  CAN packet fragmentation/reassembly plus calls into the existing session.

### Important CAN boundary for later

Do not attempt to put a 148-byte protocol packet directly into an 8-byte
classic CAN frame. The future CAN glue will need its own fragmentation,
reassembly, addressing, timeout, duplicate, and flow-control rules. Those rules
belong below the logical update packet/session engine and should be designed
when the actual board and intended network behavior can be tested.

## Phase 5 — Enforce Image and Product Compatibility

Goal: make the device, not only the host tool, decide whether an image is safe
to run.

- [ ] Define stable numeric product, board, and hardware-revision identifiers.
- [ ] Validate compatibility in the bootloader before activation.
- [ ] Validate the expected target slot/link address or otherwise make slot
      compatibility unambiguous.
- [ ] Define upgrade and downgrade policy.
- [ ] Make `BEGIN` reject incompatible metadata before erasing a slot where
      practical; always re-check the finalized manifest after transfer.
- [ ] Add wrong-board, wrong-product, wrong-slot, unsupported-manifest-version,
      and downgrade tests.
- [ ] Keep human-readable names in JSON for user interfaces, not as the trusted
      compatibility identity.

Acceptance criteria:

- A modified or bypassed host updater cannot activate an incompatible image.
- Compatibility decisions and failure statuses are covered by bootloader tests.

## Phase 6 — Add Firmware Authenticity

Goal: prevent unauthorized firmware installation before field deployment.

- [ ] Specify exactly which manifest fields and payload bytes are signed.
- [ ] Select a signature scheme and small, reviewed verification library.
- [ ] Define key identifiers and public-key provisioning in the bootloader.
- [ ] Version the manifest format for the signed representation.
- [ ] Update finalization tooling to sign release artifacts.
- [ ] Reject unsigned and incorrectly signed images in production builds.
- [ ] Decide whether development builds use a development key or an explicit
      insecure build configuration.
- [ ] Add valid-signature, changed-payload, changed-manifest, unknown-key, and
      unsigned-image tests.

Acceptance criteria:

- CRC remains available for corruption detection, but signature verification
  is required for activation in production configuration.
- Security cannot be disabled accidentally by omitting a build argument.

## Phase 7 — Unify Board and Memory Configuration

Goal: remove duplicated constants and make board support explicit.

- [ ] Create one source of truth per board for flash regions, RAM, clocks,
      logical resources, and peripheral selections.
- [ ] Generate or derive linker symbols, C constants, Make variables, and host
      tooling metadata from that source.
- [ ] Remove duplicated flash addresses from `config.mk`, linker scripts, and
      `shared/image/flash_layout.h`.
- [ ] Add build-time assertions for alignment, overlap, erase-page boundaries,
      and total flash/RAM capacity.
- [ ] Move LED/pin mapping into the board-support package.
- [ ] Keep `nucleo-f446re` explicitly unsupported until its vendor sources,
      startup, linker configuration, and required drivers all build.

Acceptance criteria:

- Changing a slot boundary requires editing only one authoritative definition.
- Generated outputs cannot silently disagree about flash layout.
- Selecting an incomplete board fails immediately with a clear message.

## Phase 8 — Simplify Application Builds

Goal: make new applications declarative and prevent copied build logic from
diverging.

- [ ] Extract common toolchain, platform, linker, finalization, flashing, and
      test rules from `apps/vibe/Makefile`.
- [ ] Give each app a small declaration containing its name, version source,
      source files, and optional components.
- [ ] Move `APP_VERSION` out of global `config.mk` and require explicit release
      version metadata.
- [ ] Provide one command that builds the canonical relocatable application
      artifact; application developers never select a slot address.
- [ ] Add `apps/template/` or a documented generator containing only the public
      application interface and one host test.
- [ ] Add a CI smoke-test application proving the template does not depend on
      `vibe` internals.

Acceptance criteria:

- A new application does not copy the `vibe` Makefile.
- Platform source lists live in one place.
- A template application builds, finalizes, and tests as one relocatable image.

## Phase 9 — Harden Persistence and Power-Loss Behavior

Goal: verify that A/B behavior remains safe at every interruption point.

- [ ] Specify flash erase/program alignment and partial-write behavior.
- [ ] Test interruption before, during, and after both boot-state record writes.
- [ ] Test interruption during inactive-slot erase and every transfer stage.
- [ ] Handle boot-state generation counter wraparound explicitly.
- [ ] Define flash endurance expectations for boot attempts and state updates.
- [ ] Add a fault-injecting flash mock that can fail after a chosen operation.
- [ ] Add hardware tests for reset/power loss during update when test equipment
      is available.
- [x] Re-lock both STM32L1 program memory and the PE controller after a
      boot-state write so external programming remains reliable.

Acceptance criteria:

- For every injected interruption, either the old confirmed app boots or the
  device remains in recoverable update mode.
- No interruption can make a partially written image confirmed.

## Phase 10 — CAN Work When Hardware Arrives

This phase is deliberately parked. Begin it only after the previous transport
boundary exists and the board is available.

- [ ] Complete STM32F4 board support and validate clocks and pins.
- [ ] Define the real CAN HAL from verified bxCAN requirements.
- [ ] Implement and hardware-test frame TX/RX, filters, errors, and bus-off
      recovery.
- [ ] Define node identity and CAN identifier allocation.
- [ ] Implement classic-CAN fragmentation/reassembly beneath logical update
      packets.
- [ ] Add retry, timeout, duplicate, arbitration, and multi-node tests.
- [ ] Implement the CAN update transport adapter without modifying the update
      session engine.
- [ ] Run update, interruption, rollback, and recovery tests on hardware.

## Cross-Cutting Definition of Done

Every completed phase must:

- [ ] Keep all previous tests passing.
- [ ] Add tests for its new platform contract and failure paths.
- [ ] Build with `-Wall -Wextra -Werror` for host and target code where
      applicable.
- [ ] Avoid MCU headers in application code.
- [ ] Document public APIs, ownership, blocking behavior, and call context.
- [ ] Update the README if developer workflow or support status changes.
- [ ] Preserve a recoverable UART update path.

## Recommended First Work Items

Start with these small, reviewable changes rather than attempting a full
directory reorganization:

1. Add the application lifecycle header and platform-owned `main()`.
2. Convert the LED task to deadline-based, non-blocking processing.
3. Add `platform_request_reset()` and remove the STM32 header from app code.
4. Add a platform watchdog/health supervisor.
5. Implement local application boot confirmation and remove updater-driven
   confirmation.
6. Extract the update session engine from the UART polling loop.
7. Enforce session IDs and specify retry/duplicate behavior.
8. Add a fake packet transport proving transport independence.
9. Consolidate common application Make rules.
10. Add the minimal template/smoke-test application.

Items 1–3 form a good first implementation slice: they improve the developer
contract without changing flash format or the update protocol.

## Decisions to Make Before Relevant Phases

These do not need to block Phase 1:

- What exact health conditions and stabilization time permit boot confirmation?
- Should the platform use callbacks, a static task table, or another small
  cooperative scheduling API?
- How should the running app request persistent confirmation: a narrowly scoped
  shared flash service, a bootloader service entry, or a reset handoff followed
  by bootloader confirmation logic?
- Must production firmware permit downgrade, and under what authorization?
- Which product/hardware identifiers must be enforced on-device?
- Which signing and key-provisioning model is appropriate for deployment?
- When CAN hardware arrives: classic CAN only or CAN FD, how node identity is
  provisioned, and whether multiple nodes may update concurrently?
