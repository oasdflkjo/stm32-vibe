# stm32-vibe

STM32 development monorepo for the ST NUCLEO-L152RE. Minimal toolset, container-based builds, LLM-assisted development.

## Hardware

- Board: ST NUCLEO-L152RE
- MCU: STM32L152RE
- User LED: LD2, green (PA5 / Arduino D13)

## Project Layout

```text
.
├── Containerfile          # Build environment (Fedora + arm-none-eabi + gcc)
├── config.mk              # Shared CPU and SWO clock configuration
├── Makefile               # Top-level orchestrator
├── bootloader/            # Bootloader (0x08000000, 16KB)
│   ├── src/
│   ├── linker.ld
│   └── Makefile
├── apps/
│   └── vibe/              # Main application (0x08004000, 496KB)
│       ├── src/
│       │   ├── main.c
│       │   ├── led_task.c / .h
│       ├── test/
│       │   └── test_led_task.c
│       ├── linker.ld
│       └── Makefile
├── shared/
│   ├── hal/               # HAL interfaces (gpio.h, systick.h, itm.h)
│   ├── image/             # Shared application image format
│   ├── libc/              # Shared no-heap newlib syscall stubs
│   └── hal_impl/
│       ├── stm32l1/       # Real hardware implementations
│       └── mock/          # Mock implementations for unit tests
├── tools/                 # Trace map extraction and host-side decoding
├── vendor/
│   ├── cmsis-core/
│   ├── cmsis_device_l1/
│   ├── stm32l1xx_hal_driver/
│   └── unity/             # Unit test framework
└── .github/
    └── workflows/
        ├── build.yml          # CI: build firmware + run unit tests
        └── publish-image.yml  # CI: publish container image to GHCR
```

## Flash Layout

| Region      | Start        | Size   |
|-------------|--------------|--------|
| Bootloader  | `0x08000000` | 16 KB  |
| App (vibe)  | `0x08004000` | 496 KB |

The bootloader validates the app manifest, CRC, stack pointer, and reset vector
at `0x08004000` before jumping to it.

## Developer Workflow

The only required host tool is `podman` (or `docker`). No ARM toolchain installation needed.

Clone with submodules:

```sh
git clone --recurse-submodules <repo-url>
# or after cloning:
git submodule update --init --depth 1
```

Build the bootloader, selected app, and combined image inside the container:

```sh
make
```

`vibe` is the default app. Select another app with `APP=<name>`:

```sh
make APP=my_app
make test APP=my_app
```

Run unit tests (host `gcc`, no cross-compilation needed):

```sh
make test
```

With Docker instead of Podman:

```sh
make CONTAINER_ENGINE=docker CONTAINER_VOLUME_SUFFIX=
```

Open an interactive shell inside the build container:

```sh
make container-shell
```

Build outputs:

```text
build/combined.hex              ← bootloader + app merged, ready to flash
bootloader/build/bootloader.elf / .bin / trace_map.json
apps/vibe/build/vibe.elf / .bin
apps/vibe/build/swo/vibe.elf / .bin / trace_map.json
```

## Unit Testing

App logic is separated from hardware via HAL interfaces in `shared/hal/`. Tests
compile against `shared/hal_impl/mock/` using the host `gcc`, with no
cross-compiler or hardware needed. Bootloader image, CRC, and vector validation
are also tested as hardware-independent modules.

```sh
make test          # build and run all tests
```

Adding a test: create `apps/<name>/test/test_<module>.c`, link it against mock HAL + Unity in the app's `Makefile`. See `apps/vibe/test/test_led_task.c` as an example.

## Flashing

Flash both bootloader and app in one shot (requires `st-flash` on host):

```sh
make flash
```

Flash individually:

```sh
make flash-bootloader     # st-flash to 0x08000000
make flash-app            # st-flash to 0x08004000
```

The bootloader validates the complete application image and its vectors before
jumping. It disables SysTick and interrupts, clears pending NVIC state,
relocates the vector table, and enters the application with its initial stack
pointer and interrupts enabled. Invalid images leave the bootloader running for
diagnosis.

## Application Image Manifest

Every application contains a 16-byte manifest at offset `0x200` from its flash
base. It records a magic value, the exact image size, a CRC-32, and the
application version from `APP_VERSION` in `config.mk`.

After linking, `tools/finalize_image.py` creates the raw binary, calculates its
CRC with the manifest CRC field treated as zero, and patches the same manifest
into both the ELF and binary. The bootloader checks this manifest and CRC before
it validates the vector table or runs any application code. Its SWO trace
reports the accepted version and size, or the rejection reason and CRC values.

This catches incomplete flashing, corruption, an erased application, and
images linked for an incompatible layout. It is integrity checking, not secure
boot: CRC-32 does not prove who produced an image. Firmware authenticity would
require a signed manifest and a protected public key.

The finalized ELF, binary, and trace map are one build artifact set. Keep them
together because the ELF contains the patched manifest and the trace map
contains build-local event IDs.

Utility targets (run on host):

```sh
make -C apps/vibe probe         # check ST-LINK is visible
make -C apps/vibe reset         # reset the board
make -C apps/vibe erase         # mass erase flash
make -C apps/vibe debug-server  # start OpenOCD GDB server
make -C apps/vibe gdb           # start GDB session
```

`make -C bootloader gdb` works the same for debugging the bootloader.

## Observability Direction

This baseline favors lightweight runtime tracing for system-level debugging.
Interactive debugging is still useful for isolated code, but stopping one MCU
at a breakpoint changes timing and can disrupt communication with the other
MCUs, radios, sensors, or host processors. In a distributed embedded system,
that can hide or create the failure being investigated.

The intended workflow is therefore:

- Keep compact trace points enabled at important state and protocol boundaries.
- Capture one continuous timeline across the bootloader and application.
- Send fixed-width IDs and values instead of format strings.
- Decode and format records on the development machine.
- Use a debugger after the trace has narrowed the problem to a local code path.

This adds predictable, small runtime overhead while preserving the timing and
communication behavior that usually matters most in a multi-MCU system.

## Compact SWO Tracing

The trace system provides printf-style diagnostics in both the bootloader and
application without transmitting format strings or requiring developers to
maintain event IDs:

```c
TRACE("BOOT jump app");
TRACE("SWO ready");
TRACE("LED toggled count=%u", toggle_count);
```

Build and flash both images with application tracing enabled:

```sh
make flash-swo
```

Read decoded trace messages in one terminal:

```sh
./swo_print.sh
```

For another selected app, use the same `APP` value for both commands:

```sh
make flash-swo APP=my_app
APP=my_app ./swo_print.sh
```

Bootloader tracing is always built in so reset and handoff failures remain
observable. Application tracing is enabled by `ENABLE_SWO_TRACE`; without that
definition, application `TRACE(...)` calls compile to no-ops.

### How It Works

1. The `TRACE(...)` macro places its format string in the ELF-only `.trace_fmt`
   section and calls the matching zero-to-four-argument trace encoder.
2. The linker assigns each format string an offset in that section. The offset
   becomes a build-local 16-bit event ID, so call sites never contain manually
   assigned IDs. Application IDs use `0x0000-0x7FFF`; bootloader IDs use
   `0x8000-0xFFFF`.
3. After linking, `tools/extract_trace_map.py` reads the ELF symbols and format
   sections and creates:

```text
bootloader/build/trace_map.json
apps/vibe/build/swo/trace_map.json
```

4. At runtime, the MCU sends only the event ID and zero to four 32-bit
   arguments. The record also contains a sync marker, protocol version,
   argument count, and CRC-8.
5. `swo_print.sh` starts OpenOCD capture. `tools/decode_trace.py` loads both
   maps, extracts records from ITM stimulus-port packets, validates them, looks
   up each event ID, and prints one continuous boot-to-application log.

The `.trace_fmt` section remains available in the ELF for tooling but is not
included in the raw binary flashed to the MCU. IDs and the map are regenerated
on every build, so both maps must always be kept with their matching firmware.
This intentionally trades stable event IDs for minimal call sites and automatic
numbering.

Trace records are serialized with a short interrupt-safe lock. If an interrupt
tries to trace while another record is being emitted, the nested record is
dropped instead of interleaving bytes or blocking interrupts during SWO waits.

Supported conversions are `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%c`, and `%%`.
Strings, floating-point values, and 64-bit arguments are rejected by the
post-link extractor because they require a separate variable-length encoding.

## Fault Diagnostics

Both the bootloader and application install handlers for HardFault, MemManage,
BusFault, and UsageFault. Divide-by-zero trapping is enabled. A fault emits:

```text
FAULT type=4 pc=0800432A lr=08004119
FAULT cfsr=00010000 hfsr=00000000
FAULT mmfar=00000000 bfar=00000000 xpsr=61000000
```

Fault types are `1` HardFault, `2` MemManage, `3` BusFault, and `4` UsageFault.
The handler disables interrupts, recovers the trace serializer if a fault
interrupted another record, reports the stacked processor state and Cortex-M
fault registers, then stops normal execution. In a normal application build,
the active watchdog subsequently resets the MCU. `MMFAR` and `BFAR` are
reported as zero when the Cortex-M validity bits say those fault-address
registers are not meaningful.

Resolve the reported program counter against the exact matching ELF:

```sh
arm-none-eabi-addr2line -e apps/vibe/build/swo/vibe.elf -f -C 0x0800432A
```

Firmware ELFs include debug line information for this purpose. That information
is not copied into the `.bin` flashed to the MCU.

The bootloader also reports the raw STM32 reset-cause register:

```text
BOOT reset csr=0C000003
```

Run the deliberate UsageFault hardware self-test with:

```sh
make flash-fault-test
./swo_print.sh
```

This test triggers the fault before starting the watchdog, intentionally
leaving the application halted in its fault handler. Restore the normal traced
application afterward with `make flash-swo`.

## Watchdog Recovery

The application starts the STM32 independent watchdog with the nominal timeout
from `WATCHDOG_TIMEOUT_MS` in `config.mk`. The watchdog uses the internal LSI
oscillator, whose frequency varies between devices, so the configured timeout
is approximate rather than a precise deadline.

The watchdog is refreshed only in the main loop after all scheduled work has
completed:

```c
while (1) {
    led_task_run();
    watchdog_refresh();
}
```

Do not refresh it inside an individual task or interrupt. Doing so could keep
the system alive while the rest of the application is stalled. When more tasks
are added, keep one central refresh point and reach it only after every required
task has demonstrated progress.

The watchdog is frozen while a debugger halts the core. In normal execution it
recovers a blocked main loop with a hardware reset, and the bootloader reports
that reset source:

```text
BOOT reset cause=watchdog
```

Run the deliberate watchdog hardware self-test with:

```sh
make flash-watchdog-test
./swo_print.sh
```

The test intentionally stops refreshing the watchdog, so the board repeatedly
resets. Restore the normal traced application afterward with `make flash-swo`.

## CI

GitHub Actions runs on every push to `main` and on pull requests. Two steps run inside the same container job (same environment as local builds):

1. **Build firmware** — cross-compiles bootloader + app, produces `combined.hex`
2. **Run unit tests** — compiles tests with host `gcc` + mock HAL, runs them

The container image is rebuilt and pushed to GHCR automatically when `Containerfile` changes. Trigger a manual rebuild from the Actions tab → "Publish build image".

## Adding a New App

1. Create `apps/<name>/` with `src/`, `linker.ld`, and a `Makefile` (copy from `apps/vibe/`)
2. Update the flash origin in `linker.ld` to not overlap with other regions
3. Set the app's `PROJECT` name in its `Makefile`
4. Build it with `make APP=<name>` and test it with `make test APP=<name>`

## Runtime Policy

The baseline intentionally provides no dynamic heap. The shared newlib syscall
layer returns `ENOMEM` from `_sbrk()`, keeping accidental `malloc()` use from
growing into the stack. Add an explicit allocator and linker-defined heap region
when a project genuinely requires dynamic allocation.

`config.mk` is the project-level source for the expected CPU clock and SWO baud.
The CPU value must match the clock configured by the CMSIS system startup; the
same value configures OpenOCD capture.

## Vendor Source Policy

Vendor submodules are not project code. They are excluded from LLM context via `.codexignore` and marked in `.gitattributes` for GitHub language stats.

License files: `vendor/*/LICENSE.md`. Project code is under the top-level `LICENSE`.
