# stm32-vibe

This project is an experiment in STM32 development with a deliberately small toolset and heavy use of LLM assistance.

The starting target is a minimal blinky program for the ST NUCLEO-L152RE board. The first goal is not to build a full embedded framework. The goal is to prove the smallest useful path from an empty repository to firmware that toggles the on-board LED.

## Hardware

- Board: ST NUCLEO-L152RE
- MCU: STM32L152RE
- User LED: LD2, green
- LED pin: PA5 / Arduino D13

The board manual filenames and public reference links are listed in `documents/documents.md`. Local PDF copies may be kept in `documents/`, but PDFs are ignored by Git so they are not committed.

## Vendor Source Policy

This project uses targeted ST submodules for CMSIS Core, the STM32L1 device package, and the STM32L1 HAL/LL driver:

```text
vendor/cmsis-core/
vendor/cmsis_device_l1/
vendor/stm32l1xx_hal_driver/
```

These are third-party sources, not project code. They are listed in `.codexignore` so normal LLM context does not get flooded by vendor files. They are also marked as vendored in `.gitattributes` for GitHub language statistics. When a specific vendor file matters, inspect that file directly.

The build uses the STM32L152xE startup file and `system_stm32l1xx.c` directly from `vendor/cmsis_device_l1`; they are not copied into `src/`.

After cloning this repository, initialize the targeted submodules with shallow history:

```sh
git submodule update --init --depth 1
```

Current upstream sources:

```text
vendor/cmsis_device_l1
revision: a23ade4ccf14012085fedf862e33a536ab7ed8be

vendor/stm32l1xx_hal_driver
revision: feefd968fe50d42fd0f639d0215bfdc876f66f86

vendor/cmsis-core
revision: 2327f7224ff212b2436e5a4cadda3288143fd041
```

Third-party license files live in their canonical vendor locations:

```text
vendor/cmsis-core/LICENSE.md
vendor/cmsis_device_l1/LICENSE.md
vendor/stm32l1xx_hal_driver/LICENSE.md
```

The linker script is project-local and was generated for this repo from the STM32L152RE memory map: 512 KiB flash at `0x08000000` and 80 KiB RAM at `0x20000000`. It is not copied from the ST project template because the available SW4STM32 linker script contains restrictive redistribution text.

The top-level `LICENSE` applies to project code. Vendor submodules keep their upstream licenses in their own repositories.

## Development Style

This repository intentionally avoids starting with a large generated project or IDE-specific setup.

The approach:

- keep the project understandable from the files in this repo
- use plain source files, a linker script, and a small build file
- add only the tools needed to compile, flash, and debug the MVP
- document decisions as they are made
- use Codex/LLMs to inspect manuals, reason about registers, and generate small changes

## MVP

The first milestone is:

1. Build a firmware image for STM32L152RE.
2. Configure PA5 as a GPIO output.
3. Toggle PA5 in a simple loop.
4. Flash the image to the NUCLEO-L152RE.
5. See LD2 blink.

## Tooling

For reproducible local builds and CI, use the containerized build path. It needs either:

- `podman` or `docker` for containerized builds

Host tools are only needed when building without the container or when flashing/debugging hardware directly:

- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `make`
- `st-flash`
- `st-info`
- `openocd`

No STM32CubeIDE project is required for the MVP. No generated HAL project is required for the MVP.

Build:

```sh
make
```

`make` builds the firmware inside the container. Podman is the default container engine.

Explicit Podman build:

```sh
make containerized-build
```

Containerized build with Docker:

```sh
make CONTAINER_ENGINE=docker CONTAINER_VOLUME_SUFFIX= containerized-build
```

Output:

```text
build/stm32-vibe.elf
build/stm32-vibe.bin
build/stm32-vibe.map
```

The raw host build target is `make firmware`. It is used inside the container and is not the normal project entry point.

Flash with ST-LINK:

```sh
make flash
```

`make flash` uses `st-flash --reset` so the board should reset and start the program after flashing. If LD2 still does not start blinking, press the physical board `RESET` button once.

Equivalent manual command:

```sh
st-flash --reset write build/stm32-vibe.bin 0x08000000
```

Check whether the board is visible:

```sh
make probe
```

If flashing succeeds but LD2 does not blink:

- press the board `RESET` button once; this has been required on the tested setup after `st-flash`
- confirm the visible LED is `LD2`, not the ST-LINK communication LED
- confirm `st-info --probe` reports `STM32L1xx_Cat_5`
- check that solder bridge `SB21` / `LD2-LED` has not been opened on the board

Additional utility targets:

```sh
make reset
make erase
make debug-server
make gdb
```

The debugger targets are optional. The normal flow stays build/flash first; debugger use is for diagnosis when the board does not behave as expected. `make gdb` requires `arm-none-eabi-gdb` or an override such as `make GDB=gdb-multiarch gdb`.

## CI

GitHub Actions builds the firmware on pushes to `main` and on pull requests. The workflow uses the same `Containerfile` as local containerized builds:

```text
.github/workflows/build.yml
```

## Project Layout

```text
.
├── LICENSE
├── README.md
├── Containerfile
├── documents/
│   └── documents.md
├── vendor/
│   ├── cmsis-core/
│   ├── cmsis_device_l1/
│   └── stm32l1xx_hal_driver/
├── Makefile
├── linker.ld
└── src/
    ├── main.c
    └── syscalls.c
```

## First Implementation Notes

For the first blinky, the firmware can use direct register access:

- enable GPIOA clock
- set PA5 as output
- toggle PA5 output state
- delay with a simple busy loop

This keeps the MVP small and makes the hardware behavior visible without pulling in a larger abstraction layer.

