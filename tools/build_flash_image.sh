#!/usr/bin/env bash
set -euo pipefail

app="${APP:-vibe}"
app_dir="apps/${app}"
output="build/combined-slots.hex"
bin_output="build/combined-slots.bin"
flash_addr="${BOOTLOADER_FLASH_ADDR:-0x08000000}"
boot_state_addr="${BOOT_STATE_FLASH_ADDR:-0x08010000}"
boot_state_size="${BOOT_STATE_FLASH_SIZE:-0x00001000}"
slot_a_addr="${APP_SLOT_A_FLASH_ADDR:-0x08011000}"
slot_b_addr="${APP_SLOT_B_FLASH_ADDR:-0x08048000}"
flash=0

usage() {
  printf 'usage: %s [--output PATH] [--bin-output PATH] [--flash-addr ADDR] [--boot-state-addr ADDR] [--boot-state-size SIZE] [--slot-a-addr ADDR] [--slot-b-addr ADDR] [--flash]\n' "$0"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --output)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      output="$2"
      shift 2
      ;;
    --bin-output)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      bin_output="$2"
      shift 2
      ;;
    --flash-addr)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      flash_addr="$2"
      shift 2
      ;;
    --boot-state-addr)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      boot_state_addr="$2"
      shift 2
      ;;
    --boot-state-size)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      boot_state_size="$2"
      shift 2
      ;;
    --slot-a-addr)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      slot_a_addr="$2"
      shift 2
      ;;
    --slot-b-addr)
      if [ "$#" -lt 2 ]; then
        usage >&2
        exit 2
      fi
      slot_b_addr="$2"
      shift 2
      ;;
    --flash)
      flash=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

if [ ! -d "$app_dir" ]; then
  printf 'Unknown app directory: %s\n' "$app_dir" >&2
  exit 2
fi

mkdir -p "$(dirname "$output")"
mkdir -p "$(dirname "$bin_output")"

printf 'Building bootloader...\n'
make -C bootloader firmware

printf 'Building canonical %s image...\n' "$app"
make -C "$app_dir" firmware BUILD_DIR=build

boot_hex="build/bootloader.hex"
slot_a_hex="build/${app}-slot-a.hex"
slot_b_hex="build/${app}-slot-b.hex"

printf 'Creating Intel HEX images...\n'
arm-none-eabi-objcopy -O ihex bootloader/build/bootloader.elf "$boot_hex"
arm-none-eabi-objcopy -I binary -O ihex --change-addresses "$slot_a_addr" "$app_dir/build/${app}.bin" "$slot_a_hex"
arm-none-eabi-objcopy -I binary -O ihex --change-addresses "$slot_b_addr" "$app_dir/build/${app}.bin" "$slot_b_hex"

grep -v ':00000001FF' "$boot_hex" > "$output"
grep -v ':00000001FF' "$slot_a_hex" >> "$output"
cat "$slot_b_hex" >> "$output"
arm-none-eabi-objcopy -I ihex -O binary "$output" "$bin_output"

printf 'Combined image: %s\n' "$output"
printf 'Combined binary: %s\n' "$bin_output"
printf 'Fast flash with: %s --output %s --bin-output %s --flash-addr %s --boot-state-addr %s --boot-state-size %s --slot-a-addr %s --slot-b-addr %s --flash\n' "$0" "$output" "$bin_output" "$flash_addr" "$boot_state_addr" "$boot_state_size" "$slot_a_addr" "$slot_b_addr"
printf 'Single-file binary flash: st-flash --reset write %s %s\n' "$bin_output" "$flash_addr"

if [ "$flash" -ne 0 ]; then
  boot_state_confirmed_a="$(dirname "$bin_output")/boot-state-confirmed-a.bin"
  printf 'Flashing bootloader...\n'
  st-flash write bootloader/build/bootloader.bin "$flash_addr"
  printf 'Flashing confirmed slot-A boot state...\n'
  python3 tools/boot_state_image.py --mode confirmed-a --size "$boot_state_size" --output "$boot_state_confirmed_a"
  st-flash write "$boot_state_confirmed_a" "$boot_state_addr"
  printf 'Flashing %s slot A...\n' "$app"
  st-flash write "$app_dir/build/${app}.bin" "$slot_a_addr"
  printf 'Flashing %s slot B...\n' "$app"
  st-flash write "$app_dir/build/${app}.bin" "$slot_b_addr"
  printf 'Resetting target...\n'
  st-flash reset
fi
