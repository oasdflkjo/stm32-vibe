# stm32-vibe

STM32 development monorepo for the ST NUCLEO-L152RE. Minimal toolset, container-based builds, LLM-assisted development.

## Hardware

- Board: ST NUCLEO-L152RE
- MCU: STM32L152RE
- User LED: LD2, green (PA5 / Arduino D13)

## Project Layout

```text
.
├── Containerfile          # Build environment (Fedora + arm-none-eabi toolchain)
├── Makefile               # Top-level orchestrator
├── bootloader/            # Bootloader (0x08000000, 16KB)
│   ├── src/
│   ├── linker.ld
│   └── Makefile
├── apps/
│   └── vibe/              # Main application (0x08004000, 496KB)
│       ├── src/
│       ├── linker.ld
│       └── Makefile
├── shared/                # Shared code across apps
├── vendor/
│   ├── cmsis-core/
│   ├── cmsis_device_l1/
│   └── stm32l1xx_hal_driver/
└── .github/
    └── workflows/
        ├── build.yml          # CI: build all projects
        └── publish-image.yml  # CI: publish container image to GHCR
```

## Flash Layout

| Region      | Start        | Size  |
|-------------|--------------|-------|
| Bootloader  | `0x08000000` | 16 KB |
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
build/combined.hex        ← bootloader + app merged, ready to flash
bootloader/build/bootloader.elf / .bin
apps/vibe/build/vibe.elf / .bin
```

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

## CI

GitHub Actions runs on every push to `main` and on pull requests. The build job runs directly inside the container image from GHCR — same environment as local builds, no Docker-in-Docker.

The container image is rebuilt and pushed to GHCR automatically when `Containerfile` changes. Trigger a manual rebuild from the Actions tab → "Publish build image".

## Adding a New App

1. Create `apps/<name>/` with `src/`, `linker.ld`, and a `Makefile` (copy from `apps/vibe/`)
2. Update the flash origin in `linker.ld` to not overlap with other regions
3. Add a `flash-<name>` target to the root `Makefile`
4. The root `make firmware` picks it up automatically via `$(wildcard apps/*)`

## Vendor Source Policy

Vendor submodules are not project code. They are excluded from LLM context via `.codexignore` and marked in `.gitattributes` for GitHub language stats.

License files: `vendor/*/LICENSE.md`. Project code is under the top-level `LICENSE`.

- set PA5 as output
- toggle PA5 output state
- delay with a simple busy loop

This keeps the MVP small and makes the hardware behavior visible without pulling in a larger abstraction layer.

