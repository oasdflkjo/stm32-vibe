# CAN Firmware Update Plan

## Goal

Prepare the firmware architecture for firmware update over a CAN shield while
keeping the first implementation small enough to finish and test. The target is
a product-ready baseline: robust against power loss, clear about update state,
and structured so later metadata and protocol features do not require another
flash-layout rewrite.

## Current Baseline

- Bootloader at `0x08000000`, currently reserved as 64 KB.
- Boot-state storage reserved at `0x08010000`, currently 4 KB.
- App slot A at `0x08011000`, currently 220 KB.
- App slot B at `0x08048000`, currently 220 KB.
- Reserved flash at `0x0807F000`, currently 4 KB.
- Application image has a manifest at offset `0x200`.
- The manifest stores magic, manifest version, manifest size, image size,
  CRC-32, software version, hardware ID, image flags, and reserved words.
- `tools/finalize_image.py` patches the manifest into both ELF and binary after
  link.
- Bootloader validates manifest, CRC, stack pointer, and reset vector before
  jumping.
- Bootloader and app already have compact SWO trace and fault reporting.
- `BOARD=nucleo-l152re` is the default board today.
- `BOARD=nucleo-f446re` is reserved for the CAN-capable board port, but still
  needs STM32F4 vendor sources and `shared/hal_impl/stm32f4/` code.
- A transport-neutral update packet layer exists in `shared/update/`.
- The update stream parser can extract CRC-checked packets from arbitrary byte
  streams, so UART bring-up can reuse the same protocol before CAN hardware
  arrives.

## Current Checkpoint

The branch is ready to continue from a clean protocol foundation:

- `shared/update/update_protocol.c` encodes and decodes update packets.
- `shared/update/update_stream.c` handles UART-style byte-stream resync,
  partial packets, bad CRC recovery, and back-to-back packets.
- `shared/hal/uart.h` defines a small byte-oriented UART HAL with mock and
  STM32L1 USART2 implementations.
- `bootloader/src/update_command.c` feeds UART bytes into `update_stream_t` and
  returns protocol ACK packets for decoded commands and parser errors.
- The bootloader update loop now handles a minimal transfer session:
  `BEGIN` selects and erases the inactive slot, `BLOCK` writes contiguous byte
  ranges using packet sequence as the slot offset, `END` checks completeness,
  and `VALIDATE` checks the candidate image manifest, CRC, and vectors in the
  selected slot.
- `bootloader/src/boot_flash_stm32l1.c` provides direct STM32L1 slot
  erase/program support behind a testable `boot_flash` boundary.
- `bootloader/src/boot_state_store_stm32l1.c` reads and writes two boot-state
  record copies in the reserved boot-state flash region.
- `bootloader/src/boot_policy.c` selects confirmed or pending slots, increments
  pending boot attempts before boot, marks failed pending slots bad, and
  promotes pending metadata to confirmed metadata on confirm.
- `ACTIVATE` now persists the selected inactive slot as pending after candidate
  validation.
- `apps/vibe/test/test_update_protocol.c` and
  `apps/vibe/test/test_update_stream.c` cover those layers.
- `bootloader/test/test_update_command.c` covers split-packet receive, bad CRC
  recovery reporting, unsupported command ACK status, transfer sequencing,
  inactive-slot writes, and candidate validation.
- `bootloader/test/test_boot_policy.c` covers pending-slot selection, attempt
  counting, rollback marking, and confirmation.
- The application build can now produce slot-specific images. `APP_SLOT=A`
  links at slot A, `APP_SLOT=B` links at slot B, and `make firmware-slot-b`
  writes the slot-B artifact set under `apps/vibe/build/slot-b/`.
- `tools/finalize_image.py` stamps application and board metadata. The 64-byte
  manifest carries fixed-size application and board IDs, while the generated
  JSON sidecar carries updater-facing names: `application_name` and
  `board_name`.
- The next implementation step is host-side updater tooling that reads the JSON
  sidecar, sends `BEGIN` metadata, streams `BLOCK` packets, and activates the
  candidate.
- `tools/uart_update.py` is the host-side UART updater. It reads the binary and
  JSON sidecar, verifies size/CRC/application/board metadata, streams the image
  in protocol packets, validates the candidate, and optionally activates it. It
  includes a lightweight terminal UI with connection state, progress, and
  updater logs, plus a plain-log fallback.
- The app includes a minimal UART update agent. It ACKs discovery, rejects
  direct flash-write commands while the app is running, and handles
  `ENTER_UPDATE` by ACKing and requesting a system reset so the host can
  continue with the bootloader.
- The bootloader probes UART briefly at reset before jumping a valid app, so the
  host updater can catch the bootloader after the app-requested reset.
- CAN transport should later fragment the same protocol packets into classic
  CAN frames.

## First Architecture Decisions

1. Define the flash layout before implementing CAN update code.
2. Define persistent boot state before implementing the update protocol.
3. Extend the image manifest before building the host-side updater.
4. Build the CAN driver and a minimal CAN protocol shared by bootloader, app,
   and host tooling.

These decisions are coupled. The bootloader cannot safely receive or activate
updates until it knows where candidate images live and how update state survives
reset.

## Flash Layout

Use A/B application partitioning.

Current layout:

```text
0x08000000  bootloader, 64 KB
0x08010000  boot persistent state, 4 KB
0x08011000  app slot A, 220 KB
0x08048000  app slot B, 220 KB
0x0807F000  reserved flash, 4 KB
0x08080000  end of flash
```

The exact sizes need to be chosen after checking:

- Final expected bootloader size with CAN, flash writer, protocol, and
  diagnostics.
- Whether both app slots can fit the expected application size.
- STM32L152RE flash page size and erase constraints.
- Whether persistent boot state should live in a dedicated flash page.

Rules:

- Bootloader region must have growth headroom.
- Each slot must be independently erasable and validatable.
- The active slot must remain untouched while downloading to the inactive slot.
- A reset or power loss during download must keep the old active app bootable.
- Slot metadata must identify which slot is active, pending, confirmed, or bad.

## Persistent Boot State

Add a small flash-backed boot state record. Store at least two copies or use a
generation counter so interrupted writes cannot brick the device.

Minimum fields:

- Magic and state format version.
- Active slot.
- Pending slot.
- Boot attempt counter for pending images.
- Confirmed version or confirmed image identity.
- Per-slot status: empty, valid, pending, confirmed, bad.
- Generation counter.
- Record CRC.
- Reserved words for future state.

Boot policy:

- Boot confirmed active image by default.
- If a pending image exists, boot it with a limited attempt count.
- The app must explicitly confirm itself after it has initialized enough to be
  considered healthy.
- If the pending app does not confirm within the attempt limit, roll back to the
  last confirmed image.
- If no valid image exists, remain in bootloader update mode.

## Image Manifest

Keep the versioned 64-byte metadata block at the fixed offset from the image
base unless there is a strong reason to move it.

Minimum fields:

- Manifest magic.
- Manifest format version.
- Manifest size.
- Image size.
- Image CRC-32.
- Software version.
- Hardware compatibility or board ID.
- Image flags.
- Reserved words.

Product-critical addition:

- Add image authenticity before field use. CRC-32 catches corruption but does
  not prove who produced the firmware. The lean secure option is a signature
  over the finalized image metadata and payload, with the public key compiled
  into the bootloader. This can be staged after CRC-based bring-up, but the
  manifest should reserve space and versioning for it now.

Tooling:

- Evolve `tools/finalize_image.py` into the binary stamping tool.
- The tool should patch the manifest after link and emit a machine-readable
  sidecar describing the image: version, size, CRC, slot compatibility, and
  later signature.
- Keep stamped ELF, binary, and trace map together as one build artifact set.

## CAN Shield Driver

The selected CAN hardware is the Waveshare RS485 CAN Shield. It provides the
CAN transceiver and expects the MCU to provide the CAN controller. The
NUCLEO-F446RE is the target board for this path.

Driver work is therefore the native STM32F4 CAN peripheral path, not an MCP2515
SPI controller path.

Driver layers:

- Low-level bus/peripheral driver.
- CAN frame send/receive API.
- Acceptance filters.
- Error state reporting.
- Bus-off recovery policy.
- Unit-testable protocol layer above the hardware driver.

Do not put update protocol details in the low-level driver.

## Bootloader CAN Support

The bootloader needs a small CAN update mode.

Responsibilities:

- Initialize CAN shield/peripheral.
- Advertise bootloader/update availability.
- Receive update commands.
- Erase and program only the inactive slot.
- Validate the candidate image after download.
- Mark the candidate slot pending.
- Reboot into the pending image.
- Stay recoverable if the app slot is invalid or update is interrupted.

Bootloader code size matters. Keep the bootloader protocol minimal and avoid
duplicating app-level services that are not needed for recovery.

## Application CAN Support

The app should share the CAN driver and protocol framing, but it should not
directly replace itself in-place.

Responsibilities:

- Normal CAN application behavior.
- Receive or forward update intent.
- Optionally download update data into the inactive slot if the bootloader flash
  writer is exposed as shared code and the safety rules are identical.
- Request reboot to bootloader/update mode when needed.
- Confirm successful boot after startup health checks.
- Report current firmware version and slot state.

Lean first version:

- App receives an update-start command and reboots to bootloader update mode.
- Bootloader performs the actual slot erase/program/validate flow.

This keeps the first product-ready path simpler and makes recovery behavior
easier to reason about.

## CAN Update Protocol

Use the transport-neutral packet format in `shared/update/` rather than ad hoc
transport-specific command IDs.

Packet fields:

- Node identity and addressing.
- Protocol version.
- Command ID.
- Transfer/session ID.
- Sequence number or block index.
- Payload length.
- Response/status code.
- CRC-32 over packet header and payload.

Minimum commands:

- Discover node and report versions.
- Enter update mode.
- Begin update with image metadata.
- Send data block.
- End transfer.
- Validate candidate.
- Activate candidate.
- Confirm running app.
- Abort update.
- Query status.

Reliability rules:

- Every state-changing command gets an acknowledgement.
- Data transfer is idempotent by block index.
- Lost frames can be retried by the host.
- Duplicate blocks are accepted if they match already-written data.
- Protocol has explicit timeout and abort behavior.
- Update mode remains reachable after reset.

CAN payload is small, so define block/chunk behavior early. For classic CAN,
the protocol must handle 8-byte frames. If CAN FD is not guaranteed, do not
depend on larger frames.

UART can carry the same full packet as a byte stream. The UART transport should
use `update_stream_t` for resynchronization and CRC rejection.

## Host Update Tool

Create a host-side update tool after the protocol skeleton exists.

Responsibilities:

- Read stamped firmware binary and metadata sidecar.
- Discover target node.
- Check hardware compatibility and current version.
- Start update session.
- Stream blocks with retries.
- Validate and activate.
- Watch reboot and confirmation.
- Print clear failure reasons.

The tool should refuse to flash an image with missing or incompatible metadata.

## Absolutely Crucial Additions

These are the extra items that should not be skipped for a product-ready update
path:

- Power-loss safety: old confirmed image must survive interrupted update.
- Rollback: pending image must confirm itself or bootloader returns to previous
  confirmed image.
- Authenticity: reserve manifest structure now and add signature validation
  before real deployment.
- Hardware compatibility: prevent flashing firmware built for the wrong board
  or CAN-shield wiring.
- Bootloader update-mode entry: define how the app requests bootloader update
  mode and how a service tool can force it.
- CAN node addressing: updates must target one device without disturbing others
  on the bus.
- Flash write discipline: erase/program alignment, page boundaries, and
  interrupted state writes must be specified.
- Test strategy: protocol, manifest, boot-state, and rollback behavior need host
  tests before hardware testing.

## Suggested Implementation Order

1. Choose flash map and slot sizes.
2. Add versioned manifest structure with reserved fields.
3. Add persistent boot state module with host unit tests.
4. Update bootloader validation to understand slots and boot state.
5. Add rollback and app-confirm flow without CAN.
6. Add UART HAL/mock and bootloader update command loop.
7. Add mock flash writer and connect update blocks to inactive-slot writes.
8. Add host UART update tool.
9. Add STM32F4 vendor sources and `nucleo-f446re` HAL implementations.
10. Add native CAN driver and CAN transport for the existing update protocol.
11. Add bootloader CAN update mode.
12. Add app integration: status reporting, update-mode request, and boot
    confirmation.
13. Add signature validation when the CRC-based path is working.

## Open Questions

- What UART instance/pins should be used for the development update transport
  on the current NUCLEO-L152RE?
- Will final field updates use classic CAN only, or can we rely on CAN FD?
- What node ID source should be used: compile-time config, flash state, DIP
  switches, or CAN command provisioning?
- What maximum application size do we need to reserve per slot?
- Should firmware downgrade be allowed?
- What health condition is enough for the app to confirm a pending image?
- Do we need encrypted firmware, or only signed firmware?
