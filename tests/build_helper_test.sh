#!/usr/bin/env bash
set -euo pipefail

repository=$(realpath -- "$(dirname -- "${BASH_SOURCE[0]}")/..")
fixtures=$(mktemp -d /tmp/microbit-flash-guard.XXXXXX)
trap 'rm -rf -- "$fixtures"' EXIT
mkdir -p "$fixtures/v1" "$fixtures/v2" "$fixtures/unknown"
printf 'Unique ID: 990000001234\r\n' > "$fixtures/v1/DETAILS.TXT"
printf 'Unique ID: 990600001234\r\n' > "$fixtures/v2/DETAILS.TXT"
printf 'Unique ID: 123400001234\r\n' > "$fixtures/unknown/DETAILS.TXT"

assert_rejected() {
  local version="$1"
  local mount="$2"
  local output
  if output=$(bash "$repository/scripts/microbit.sh" "$version" --flash "$mount" 2>&1); then
    printf 'FAIL: accepted mismatched/unknown board %s at %s\n' "$version" "$mount" >&2
    exit 1
  fi
  if [[ "$output" != *"Refusing $version flash:"* ]]; then
    printf 'FAIL: rejection was not the board-identity guard: %s\n' "$output" >&2
    exit 1
  fi
  if compgen -G "$mount/*.hex" >/dev/null; then
    printf 'FAIL: firmware was written to a rejected board.\n' >&2
    exit 1
  fi
}

assert_rejected v2 "$fixtures/v1"
assert_rejected v1 "$fixtures/v2"
assert_rejected v1 "$fixtures/unknown"
assert_rejected v2 "$fixtures/unknown"
if bash "$repository/scripts/microbit.sh" invalid >/dev/null 2>&1; then
  printf 'FAIL: accepted an invalid board target.\n' >&2
  exit 1
fi
printf 'PASS: mismatched and unknown board flash protection; invalid target rejection.\n'