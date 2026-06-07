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
│       │   └── syscalls.c
│       ├── test/
│       │   └── test_led_task.c
│       ├── linker.ld
│       └── Makefile
├── shared/
│   ├── hal/               # HAL interfaces (gpio.h, systick.h)
│   └── hal_impl/
│       ├── stm32l1/       # Real hardware implementations
│       └── mock/          # Mock implementations for unit tests
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

The bootloader validates the app at `0x08004000`, sets the vector table offset, then jumps to it.

## Developer Workflow

The only required host tool is `podman` (or `docker`). No ARM toolchain installation needed.

Clone with submodules:

```sh
git clone --recurse-submodules <repo-url>
# or after cloning:
git submodule update --init --depth 1
```

Build everything (bootloader + app + combined hex) inside the container:

```sh
make
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
bootloader/build/bootloader.elf / .bin
apps/vibe/build/vibe.elf / .bin
```

## Unit Testing

App logic is separated from hardware via HAL interfaces in `shared/hal/`. Tests compile against `shared/hal_impl/mock/` using the host `gcc` — no cross-compiler, no hardware needed.

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

Utility targets (run on host):

```sh
make -C apps/vibe probe         # check ST-LINK is visible
make -C apps/vibe reset         # reset the board
make -C apps/vibe erase         # mass erase flash
make -C apps/vibe debug-server  # start OpenOCD GDB server
make -C apps/vibe gdb           # start GDB session
```

`make -C bootloader gdb` works the same for debugging the bootloader.

## Compact SWO Tracing

Build and flash the app with SWO tracing enabled:

```sh
make -C apps/vibe flash-swo
```

Read decoded trace messages in one terminal:

```sh
./swo_print.sh
```

Trace calls keep the familiar format-string shape:

```c
TRACE(LED_TOGGLED, "LED toggled count=%u", toggle_count);
```

Before firmware compilation, `tools/gen_trace.py` scans the app sources and
generates:

```text
apps/vibe/build/swo/generated/trace_ids.h
apps/vibe/build/swo/trace_map.json
```

The generated header contains only deterministic 16-bit event IDs and typed
call macros. Format strings remain in the host-side JSON map and are not linked
into the firmware.

Each firmware record contains a sync marker, protocol version, event ID,
argument count, zero to four 32-bit arguments, and a CRC-8. The host decoder
extracts ITM stimulus-port packets, validates each record, and formats it using
the generated map.

Supported conversions are `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%c`, and `%%`.
Strings, floating-point values, and 64-bit arguments are rejected by the
generator because they require a separate variable-length encoding.

## CI

GitHub Actions runs on every push to `main` and on pull requests. Two steps run inside the same container job (same environment as local builds):

1. **Build firmware** — cross-compiles bootloader + app, produces `combined.hex`
2. **Run unit tests** — compiles tests with host `gcc` + mock HAL, runs them

The container image is rebuilt and pushed to GHCR automatically when `Containerfile` changes. Trigger a manual rebuild from the Actions tab → "Publish build image".

## Adding a New App

1. Create `apps/<name>/` with `src/`, `linker.ld`, and a `Makefile` (copy from `apps/vibe/`)
2. Update the flash origin in `linker.ld` to not overlap with other regions
3. Add a `flash-<name>` target to the root `Makefile`
4. The root `make firmware` and `make test` pick it up automatically via `$(wildcard apps/*)`

## Vendor Source Policy

Vendor submodules are not project code. They are excluded from LLM context via `.codexignore` and marked in `.gitattributes` for GitHub language stats.

License files: `vendor/*/LICENSE.md`. Project code is under the top-level `LICENSE`.
3. Add a `flash-<name>` target to the root `Makefile`
4. The root `make firmware` picks it up automatically via `$(wildcard apps/*)`

## Vendor Source Policy

Vendor submodules are not project code. They are excluded from LLM context via `.codexignore` and marked in `.gitattributes` for GitHub language stats.

License files: `vendor/*/LICENSE.md`. Project code is under the top-level `LICENSE`.

- set PA5 as output
- toggle PA5 output state
- delay with a simple busy loop

This keeps the MVP small and makes the hardware behavior visible without pulling in a larger abstraction layer.
