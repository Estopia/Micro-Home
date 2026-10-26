#!/usr/bin/env bash
set -euo pipefail

usage() {
  printf 'Usage: bash scripts/microbit.sh v1|v2 [--battery] [--flash MICROBIT_MOUNT]\n'
}

if [[ "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

version="${1:-}"
case "$version" in
  v1) board="bbc_microbit/nrf51822"; application="firmware-v1" ;;
  v2) board="bbc_microbit_v2/nrf52833"; application="firmware" ;;
  *) usage >&2; exit 2 ;;
esac
shift

profile="debug"
mount=""
while (( $# > 0 )); do
  case "$1" in
    --battery) profile="battery"; shift ;;
    --flash)
      if (( $# < 2 )); then usage >&2; exit 2; fi
      if [[ -z "$2" ]]; then usage >&2; exit 2; fi
      mount="$2"
      shift 2
      ;;
    *) usage >&2; exit 2 ;;
  esac
done

if [[ -n "$mount" ]]; then
  if [[ ! -f "$mount/DETAILS.TXT" || ! -w "$mount" ]]; then
    printf 'Not a writable MICROBIT drive: %s\n' "$mount" >&2
    exit 1
  fi
  identity=$(awk '/^Unique ID: / { sub(/\r$/, ""); print $3; exit }' "$mount/DETAILS.TXT")
  case "$version:$identity" in
    v1:9900*|v2:9903*|v2:9904*|v2:9905*|v2:9906*) ;;
    *)
      printf 'Refusing %s flash: unsupported or mismatched DAPLink ID %s\n' "$version" "$identity" >&2
      exit 1
      ;;
  esac
fi

: "${ZEPHYR_BASE:?Set ZEPHYR_BASE to the Zephyr 4.2.0 checkout}"
command -v west >/dev/null
repository=$(realpath -- "$(dirname -- "${BASH_SOURCE[0]}")/..")
build_root="${MICROBIT_BUILD_ROOT:-${TMPDIR:-/tmp}/microbit-bthome-build-${UID}}"
if [[ "$build_root" =~ [[:space:]] ]]; then
  printf 'MICROBIT_BUILD_ROOT must not contain whitespace.\n' >&2
  exit 1
fi
source_dir="$build_root/source"
build_dir="$build_root/$version-$profile"
mkdir -p "$source_dir"
cp -a "$repository/firmware" "$repository/firmware-v1" "$repository/components" "$source_dir/"

arguments=()
if [[ "$profile" == "battery" ]]; then
  arguments+=(-- -DEXTRA_CONF_FILE=battery.conf)
fi
west build -p always -b "$board" -d "$build_dir" "$source_dir/$application" "${arguments[@]}"
image="$build_dir/zephyr/zephyr.hex"
printf '\nBuilt %s (%s): %s\n' "$version" "$profile" "$image"

if [[ -n "$mount" ]]; then
  current_identity=$(awk '/^Unique ID: / { sub(/\r$/, ""); print $3; exit }' "$mount/DETAILS.TXT")
  if [[ "$current_identity" != "$identity" ]]; then
    printf 'Connected board changed during the build; refusing to flash.\n' >&2
    exit 1
  fi
  cp -- "$image" "$mount/microbit-$version-bthome.hex"
  sync -f "$mount"
  printf 'Copied fresh %s firmware to %s (DAPLink %s). Check serial/BLE to verify boot.\n' "$version" "$mount" "$identity"
fi