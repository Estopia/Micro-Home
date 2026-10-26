# Micro-Home

Turn a supported BBC micro:bit v1 or v2 into a wireless sensor for Home Assistant.
The micro:bit broadcasts BTHome V2 packets; an ESP32 running ESPHome receives
them passively and exposes their values over Wi-Fi. No pairing, GATT connection,
micro:bit Wi-Fi hardware or Home Assistant Bluetooth proxy is needed.

```text
micro:bit sensors  -- BLE advertisements -->  ESP32 / ESPHome  -- Wi-Fi -->  Home Assistant
```

| Entity | Value | Important limitation |
| --- | --- | --- |
| Die Temperature | Degrees Celsius | Chip temperature, not calibrated room temperature |
| Total Acceleration | m/s^2, including gravity | Approximately 9.81 when stationary |
| Button A and Button B | Pressed / released | Five-second snapshots; short taps can be missed |
| Compass Heading | 0-359.99 degrees | Uncalibrated magnetic heading; hold the board level |

**This is a periodic sensor/state monitor, not a reliable button-click remote.**
BLE packets are unencrypted and unauthenticated. Do not use them for security-
or safety-critical decisions. Battery life and compass accuracy are not certified.

## Compatibility

- Firmware: **Zephyr 4.2.0**, **Zephyr SDK 0.17.2**, **CMake 3.31.6**,
  boards `bbc_microbit/nrf51822` (v1) and `bbc_microbit_v2/nrf52833` (v2).
  Uses Zephyr's Bluetooth controller/host, not a preinstalled SoftDevice.
- V1 hardware: early **v1.3** boards with **MMA8653FC + MAG3110**, DAPLink ID
  prefix `9900`. Separate application in [firmware-v1/CMakeLists.txt](firmware-v1/CMakeLists.txt),
  with a local MAG3110 driver and smaller stacks for the nRF51822's 16 KB RAM.
  Later v1.5 sensor variants are not supported by this target.
- V2 hardware: micro:bit v2 with **LSM303AGR** accelerometer, as defined by the
  upstream Zephyr board. The alternate **FXOS8700** fitting is not supported by
  this configuration. Check the fitted chip before deploying; a missing sensor
  is a startup error, not a fabricated zero reading.
- Gateway: **ESPHome 2026.10.0**, original ESP32 development board (`esp32dev`),
  or ESP32-C3 (`esp32-c3-devkitm-1`), with the ESP-IDF framework.
  ESP32-S2 has no Bluetooth and cannot be used. Other variants are not verified here.
- Standard micro:bit MicroPython does **not** expose the standard `bluetooth`
  BLE API alongside `microbit`; this is why the firmware uses Zephyr C++.
- ESPHome 2026.10.0 has no upstream `bthome` platform. The supplied
  **local external component** implements the sensor and binary-sensor platforms.
  Keep `components/bthome` beside the gateway YAML. It is a deliberately strict
  receiver for this transmitter's profile, not a general BTHome decoder.

## Start Here: Computer Setup

You need a supported micro:bit, a data-capable USB cable, a BLE-capable ESP32,
2.4 GHz Wi-Fi and Home Assistant. No external micro:bit sensors or wiring are
required. Start on USB power; use a battery build after verifying reception.

These instructions target **Linux with Bash** and Python **3.12**. The helper uses
Linux utilities including `realpath` and `sync -f`; Windows and macOS workflows
have not been validated. Use the pinned tool versions before trying upgrades.

Install Git, a native C/C++ compiler, Make, archive utilities and the other
[Zephyr Linux prerequisites](https://docs.zephyrproject.org/4.2.0/develop/getting_started/index.html)
using your distribution's instructions. Install
[uv](https://docs.astral.sh/uv/getting-started/installation/) for Python environment
and package management. Then clone this repository and create an environment:

```bash
git clone https://github.com/Estopia/Micro-Home.git "$HOME/Micro-Home"
uv venv --python 3.12 "$HOME/.venvs/micro-home"
source "$HOME/.venvs/micro-home/bin/activate"
uv pip install 'west==1.5.0' 'cmake==3.31.6' ninja 'esphome==2026.10.0'
```

Create a **separate Zephyr workspace** and install the ARM toolchain. Downloads
can take time and require several gigabytes of free disk space:

```bash
west init --mr v4.2.0 --clone-opt=--depth=1 "$HOME/zephyrproject-micro-home"
cd "$HOME/zephyrproject-micro-home"
west update --narrow -o=--depth=1 hal_nordic hal_st cmsis cmsis_6
uv pip install -r zephyr/scripts/requirements-base.txt
export ZEPHYR_BASE="$PWD/zephyr"
west sdk install --version 0.17.2 --toolchains arm-zephyr-eabi --install-dir "$HOME/zephyr-sdk-0.17.2"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.17.2"
cd "$HOME/Micro-Home"
```

In each new terminal, reactivate the environment and paths:

```bash
source "$HOME/.venvs/micro-home/bin/activate"
export ZEPHYR_BASE="$HOME/zephyrproject-micro-home/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.17.2"
cd "$HOME/Micro-Home"
```

Avoid spaces in the Zephyr workspace and SDK paths. Use CMake **3.31.6**:
Zephyr 4.2's SDK discovery failed with CMake 4.x during testing.

## Choose and Deploy a Board

Both applications share the tested packet encoder, compass maths and sampling
loop. Their devicetree, drivers and memory configurations are separate; adding v1
does not replace the working v2 application in `firmware/`.

After installing the pinned Zephyr toolchain described below, run from this
repository root with `west` on PATH and `ZEPHYR_BASE` set:

```sh
bash scripts/microbit.sh v1
bash scripts/microbit.sh v2
bash scripts/microbit.sh v1 --battery
bash scripts/microbit.sh v2 --battery
```

Connect your micro:bit and locate its mounted **MICROBIT** drive, containing
`DETAILS.TXT`. Typical Linux locations are `/media/<username>/MICROBIT` and
`/run/media/<username>/MICROBIT`. To build and deploy, set the actual mount path
and run the command matching your board:

```bash
MICROBIT_MOUNT="/media/$USER/MICROBIT"
bash scripts/microbit.sh v1 --flash "$MICROBIT_MOUNT"
```

For a supported v2, use:

```bash
bash scripts/microbit.sh v2 --flash "$MICROBIT_MOUNT"
```

Run only the command matching the connected board. The helper checks DAPLink's
`DETAILS.TXT`, refuses unknown/mismatched board IDs, and rechecks the identity after
building. It always builds before copying the matching HEX image. Flashing replaces
the board's existing application. The helper never flashes the ESP32.

Sources and build outputs are staged under `/tmp/microbit-bthome-build-$UID` to
avoid Zephyr's path-with-spaces issue. Override with a space-free
`MICROBIT_BUILD_ROOT`. Each board/profile has its own build directory. Serial
logging is available at 115200 baud for debug builds, but disabled for battery
builds. A copied HEX must still be verified through serial startup and BLE.
Do not run concurrent builds into the same build root.

The flash guard accepts early-v1 DAPLink prefix `9900` for `v1` and prefixes
`9903` through `9906` for `v2`; it cannot detect the fitted sensor chip.
Unknown or mismatched IDs are refused. Do not bypass this by installing an image
for another MCU. Both targets use the same BTHome decoder, but every board has a
different BLE address: read **your board's MAC**, then configure it in the gateway.

## Part 1: Firmware

Implementation: [firmware/src/main.cpp](firmware/src/main.cpp), configuration:
[firmware/prj.conf](firmware/prj.conf), packet format:
[components/bthome/protocol.h](components/bthome/protocol.h).
The v1 application builds the same main loop with its own board overlay and
[MAG3110 driver](firmware-v1/src/mag3110.cpp). Its driver verifies chip ID 0xC4,
uses 1.25 Hz continuous magnetic sampling, checks data-ready, and converts signed
big-endian measurements at 0.1 microtesla/count into Zephyr's gauss units.

### Exact advertising bytes

Example: 25.00 degrees C, A held, B released, acceleration 9.807 m/s^2,
magnetic heading 90.00 degrees:

```text
02 01 06  11 16 D2 FC 40  02 C4 09  0F 01  0F 00  51 4F 26  5E 28 23
```

| Bytes | Meaning |
| --- | --- |
| `02 01 06` | Flags AD: length 2, type 0x01, General Discoverable + no BR/EDR |
| `11 16` | Service-data AD: 17 following bytes including type 0x16 |
| `D2 FC` | BTHome service UUID 0xFCD2, little-endian |
| `40` | Version 2; unencrypted; regular updates; reserved bits zero |
| `02 C4 09` | Temperature object 0x02; signed LE int16 2500, factor 0.01 |
| `0F 01` | Generic boolean object 0x0F; Button A pressed=1, released=0 |
| `0F 00` | Second generic boolean; Button B pressed=1, released=0 |
| `51 4F 26` | Acceleration object 0x51; unsigned LE uint16 9807, factor 0.001 |
| `5E 28 23` | Direction object 0x5E; unsigned LE uint16 9000, factor 0.01 degree |

Object IDs are non-decreasing. Repeated booleans are always A then B.
The entire advertisement is **21 bytes**, below the
legacy BLE 31-byte limit. There is no optional packet-ID object (0x00),
encryption counter or MIC. Device info 0x40 does not itself encode a counter flag.
The encoder explicitly constructs both AD elements; the firmware gives their
type/data fields to Zephyr, which serialises the same length/type bytes on air.
There is no scan response or separate UUID-list AD element; the UUID is carried
in service data, which passive scanners can read.

**Why not 0x3A?** That object represents button *events*: `3A 01` is a press;
`3A 00` means no event, **not release**. A held/released entity needs 0x0F.
No 0x3A events are transmitted by this implementation.

### Timing and accuracy

- Sample every 5 seconds, then advertise for 600 ms at 100-110 ms intervals,
  plus BLE's random advertising delay, on channels 37/38/39. Several identical
  copies improve reception; this is not Bluetooth periodic advertising.
- Advertising stops between bursts. `k_msleep()` and the tickless idle kernel
  allow CPU System ON idle sleep, not busy waiting or System OFF. The
  accelerometer and magnetometer remain enabled; the LED matrix is disabled.
  The v2 accelerometer runs at 1 Hz; the v1 uses its MMA8653 driver defaults and
  a 1.25 Hz MAG3110 compass. The five-second BLE schedule is identical.
- Both button GPIOs are active-low and converted to logical pressed/released by Zephyr.
  Two matching readings 25 ms apart debounce each sample. A bouncing sample is
  skipped. **Taps entirely between samples are not captured**; this is a periodic
  state monitor, not a reliable click-event transport. BLE delivery is best effort.
- Total acceleration is `sqrt(ax*ax + ay*ay + az*az)` in m/s^2, including gravity:
  approximately 9.81 at rest, not zero. The v2 range is +/-4 g per axis; v1 uses
  the MMA8653 driver's defaults.
- Compass heading is `atan2(-mag_y, mag_x)`, normalised to 0-359.99 degrees.
  It describes the magnetometer's +X axis relative to **magnetic**, not true, north.
  Hold the board level: this is a two-axis compass, **not tilt compensated**.
  No claim is made that the reference axis is the printed top edge of the board.
  Keep away from magnets, speakers, steel and high-current wiring. Defaults are
  uncalibrated. For basic hard-iron correction, measure each magnetic axis's
  minimum and maximum during a full level rotation and set
  `MAGNETIC_X_OFFSET_GAUSS` and `MAGNETIC_Y_OFFSET_GAUSS` in the firmware to
  `(maximum + minimum) / 2`, in gauss, then rebuild. Soft-iron distortion requires
  additional calibration; declination and tilt compensation are not implemented.
  Recording those raw X/Y values currently requires temporary firmware
  instrumentation: there is no built-in calibration UI or raw magnetic-field
  entity. Calibrate each board separately; the shared constants affect whichever
  board target you rebuild.
- Temperature is the **nRF die temperature**, not a calibrated ambient sensor.
  Native resolution is 0.25 degrees C; encoding at 0.01 does not improve it.
  Calibrate against a reference in the intended enclosure if necessary.
- Sensor errors skip that transmission. BLE start/stop errors trigger a reboot.
  Fatal initialisation failures are logged and require fixing the hardware/config.

### Build and flash

Use an activated Python virtual environment with `west`, `cmake==3.31.6`, `ninja`
and Zephyr's `scripts/requirements-base.txt` installed. Initialise a Zephyr
workspace at tag `v4.2.0` using the [official setup guide](https://docs.zephyrproject.org/4.2.0/develop/getting_started/index.html).
For this application the required manifest projects are `hal_nordic`, `hal_st`,
`cmsis` and `cmsis_6`; installing all manifest projects is also supported.

From the Zephyr workspace, install the SDK once:

```sh
export ZEPHYR_BASE="$PWD/zephyr"
west update --narrow -o=--depth=1 hal_nordic hal_st cmsis cmsis_6
west sdk install --version 0.17.2 --toolchains arm-zephyr-eabi --install-dir "$HOME/zephyr-sdk-0.17.2"
export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.17.2"
```

Stage the project from its root if the source path contains spaces (Zephyr 4.2's
overlay handling splits such paths). Build outputs should also have no spaces:

```sh
source_dir=$(mktemp -d /tmp/microbit-source.XXXXXX)
cp -a firmware components "$source_dir/"
west build -b bbc_microbit_v2/nrf52833 -d /tmp/microbit-build "$source_dir/firmware"
west flash -d /tmp/microbit-build --runner pyocd
```

These manual commands target v2. For v1, also stage `firmware-v1`, select
`bbc_microbit/nrf51822` and use the `firmware-v1` application path with a separate
build directory. The helper above performs those selections and handles staging.

Install `pyocd` in the same environment for flashing. Alternatively, drag
`/tmp/microbit-build/zephyr/zephyr.hex` onto the MICROBIT USB mass-storage drive.
Flashing replaces the existing micro:bit application, including MicroPython.

### MAC address and battery deployment

Open the micro:bit USB serial port at **115200, 8N1** and reset the board:

```sh
ls -l /dev/serial/by-id/
python -m serial.tools.miniterm /dev/ttyACM0 115200
```

Replace `/dev/ttyACM0` with your board's serial port. On permission errors, follow
your distribution's serial-device access guidance rather than running the whole
toolchain as root. Open the terminal before resetting so you see the startup log.

The startup log contains `BTHome MAC: XX:XX:XX:XX:XX:XX (random)`.
Copy the six octets, without `(random)`, into `microbit_mac` in the gateway YAML.
The code derives a static-random identity from the nRF factory DEVICEADDR,
sets the two static-address bits, and uses `BT_LE_ADV_OPT_USE_IDENTITY`.
It remains stable across resets and builds; it is not a rotating private address.

Once the MAC is recorded, disable the UART and logging for battery use. Select
`v1` or `v2` to match the connected board:

```bash
bash scripts/microbit.sh v1 --battery --flash "$MICROBIT_MOUNT"
```

Flash that newly built image. The battery image does not print its MAC. Measure
whole-board current on battery with USB/debug probes disconnected: the interface
processor and other board circuitry affect consumption. No battery-life or
microamp-current claim is made without measurement.

## Part 2: ESP32 Gateway

Configuration: [microbit_gateway.yaml](microbit_gateway.yaml).

Create your local secrets and generate a new API key:

```bash
cp secrets.example.yaml secrets.yaml
openssl rand -base64 32
```

1. Copy [secrets.example.yaml](secrets.example.yaml) to `secrets.yaml` and set Wi-Fi
   credentials and a long unique OTA password. The example API key is public test
   data: **replace it** with the output of `openssl rand -base64 32`.
2. Set `microbit_mac` to the serial-reported address. Adjust `board` only if using
   a different BLE-capable ESP32 board.
3. Keep `components/bthome/` beside the YAML. It is required by the local
  `external_components` declaration; copying just the YAML is not sufficient.
4. With the environment above active, validate/build/flash:

```bash
esphome config microbit_gateway.yaml
esphome compile microbit_gateway.yaml
esphome run microbit_gateway.yaml
```

Use USB for the first ESP32 installation. Later updates can use ESPHome OTA.
Never commit real credentials: `secrets.yaml` is ignored by Git. The example API
key is public test data, not a deployment secret.

For an ESP32-C3 development board, replace the gateway's `esp32` block with:

```yaml
esp32:
  board: esp32-c3-devkitm-1
  variant: esp32c3
  framework:
   type: esp-idf
```

Check your exact board before choosing another target. If using ESPHome Device
Builder, place both the YAML and local component directory in its configuration
directory, supply secrets there, and use the same pinned ESPHome version.

### Add to Home Assistant

1. Open **Settings > Devices & services** and look for the discovered ESPHome node.
2. If needed, add the **ESPHome** integration using the ESP32's IP address or
  hostname. The example hostname is `microbit-gateway.local`.
3. Supply the API encryption key from your secrets when requested.
4. Open the gateway device and check its five micro:bit entities after the next
  transmission. Hold each button longer than five seconds when testing.

Entities belong to the **ESPHome gateway device**. A separate Home Assistant
BTHome integration or Bluetooth proxy is not required. Independently receiving
the packets with Home Assistant Bluetooth may create another set of entities.

Scanning is passive (`active: false`), with a 300 ms window per 320 ms interval.
These settings suit a mains-powered gateway; Wi-Fi coexistence still creates
gaps. No pairing or connection to the micro:bit is attempted.

Entities are packet-driven: no polling `update_interval` applies. Updates are
expected approximately every 5 seconds; lost bursts can make that longer.
Both buttons have an additional 30 ms ESPHome `delayed_on_off` filter. After 30
seconds without a valid packet, numeric values become NaN and both button states
are invalidated rather than retaining stale pressed/released states indefinitely.
This marks entity data unknown, not the whole ESPHome node offline.

Upload the updated receiver component and YAML as well as flashing the transmitter:
the old receiver rejects the extended payload. The new receiver still accepts the
original one-button, no-compass packets; Button B and heading remain unknown for
those packets. The existing `button:` YAML key continues to mean Button A;
`button_b:` adds B and `heading:` adds the compass. To add this to an existing
ESPHome node, merge entries into its existing sensor lists rather than creating
duplicate top-level YAML keys.

### Receive Two Boards

To switch from one micro:bit to another, change the gateway MAC and upload again.
To receive two boards at once, replace the example's `bthome`, `sensor` and
`binary_sensor` sections with the following. Substitute your two real MACs and
keep the other gateway sections. The addresses below are placeholders.

```yaml
bthome:
  - id: microbit_v1
    mac_address: "C0:00:00:00:00:01"
    stale_timeout: 30s
  - id: microbit_v2
    mac_address: "C0:00:00:00:00:02"
    stale_timeout: 30s

sensor:
  - platform: bthome
    bthome_id: microbit_v1
    temperature:
      name: "Microbit V1 Die Temperature"
    acceleration:
      name: "Microbit V1 Total Acceleration"
    heading:
      name: "Microbit V1 Compass Heading"
  - platform: bthome
    bthome_id: microbit_v2
    temperature:
      name: "Microbit V2 Die Temperature"
    acceleration:
      name: "Microbit V2 Total Acceleration"
    heading:
      name: "Microbit V2 Compass Heading"

binary_sensor:
  - platform: bthome
    bthome_id: microbit_v1
    button:
      name: "Microbit V1 Button A"
      filters:
        - delayed_on_off: 30ms
    button_b:
      name: "Microbit V1 Button B"
      filters:
        - delayed_on_off: 30ms
  - platform: bthome
    bthome_id: microbit_v2
    button:
      name: "Microbit V2 Button A"
      filters:
        - delayed_on_off: 30ms
    button_b:
      name: "Microbit V2 Button B"
      filters:
        - delayed_on_off: 30ms
```

### MQTT alternative

The supplied configuration uses the encrypted native API. For an MQTT-only
gateway, remove the `api` block (otherwise its no-client reboot timeout applies)
and add the following, with the three additional secrets populated:

```yaml
mqtt:
  broker: !secret mqtt_broker
  username: !secret mqtt_username
  password: !secret mqtt_password
  discovery: true
```

ESPHome publishes the same entities and Home Assistant MQTT discovery. This
minimal alternative uses unencrypted MQTT on port 1883: use only on a trusted
isolated LAN, or configure broker TLS before wider deployment.

Unencrypted BTHome itself has **no authentication, confidentiality or replay
protection**. A MAC allowlist is not authentication. Do not use these values for
security-critical decisions; anyone nearby can read or spoof the advertisements.

## Part 3: Troubleshooting and Verification

- In **nRF Connect**, scan without connecting. Find the serial-reported address
  (Android), inspect **Service Data UUID 0xFCD2**, and verify that service data
  starts `40 02`. Some versions display the full 128-bit base UUID. iOS does not
  expose a peripheral's actual BLE MAC; use micro:bit serial or ESPHome logs for
  MAC matching, not iOS's peripheral identifier.
- At the example values, service data is `40 02 C4 09 0F 01 0F 00 51 4F 26 5E 28 23` when the
  app displays UUID separately, or `D2 FC` followed by those bytes otherwise.
  Ensure the service-data AD length is `11`. Hold A and B separately and together
  across successive five-second samples. Rotate level away from metal and check
  that heading changes smoothly and wraps at north; compare against a reference
  compass only after calibrating the magnetic offsets and reference-axis alignment.
- Look for `[bthome] Accepted V2 packet: temperature=... button_a=... button_b=... acceleration=... heading=... extended=1 RSSI=...`.
  Raise global logger level to `VERY_VERBOSE` and `bthome` to `VERBOSE` temporarily
  for the configured-MAC/no-service-data diagnostic. For general discovery use
  `esp32_ble_tracker: DEBUG` under `logger.logs` (global level at least DEBUG).
- These are the **actual messages from the supplied component**, not invented
  upstream BTHome logs:

```text
Dropped BTHome packet: invalid service-data length (expected 14 or legacy 9 bytes after UUID)
Dropped BTHome packet: unsupported device info (expected unencrypted V2 0x40)
Dropped BTHome packet: unexpected object layout (expected 02,0F,0F,51,5E or legacy 02,0F,51)
Dropped BTHome packet: invalid boolean (expected 0 or 1)
Dropped BTHome packet: invalid heading (expected 0 through 35999 centidegrees)
No valid BTHome packet within stale_timeout; readings unknown
```

- Wrong-MAC packets are silently ignored. Radio-corrupt packets are normally
  rejected by the controller CRC check before ESPHome sees them; no application
  log is guaranteed. Keep the micro:bit nearby, check its startup errors, check
  the MAC after reset, and keep BTHome data in the advertisement, not scan response.
- Power off the micro:bit: its entities should become unknown after 30 seconds.
  Power it back on: the MAC should remain unchanged and all states should recover.
- On battery, compare debug and battery builds with a current meter. Assess packet
  reception under real Wi-Fi load before reducing the 600 ms burst length.

### Automated checks and remaining hardware gates

Run the byte-exact, signed-value and malformed-packet tests from this project root:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -I. tests/protocol_test.cpp -o /tmp/microbit-bthome-test
/tmp/microbit-bthome-test
bash tests/build_helper_test.sh
```

The helper test uses temporary fake DAPLink drives to verify rejection of wrong
or unknown board revisions without touching connected hardware.

Verified during development: protocol/compass tests, ESPHome 2026.10.0 code
generation and full ESP32-C3 compilation, plus Zephyr 4.2.0 v1/v2 debug/battery
builds. Both micro:bit targets have been flashed and valid extended packets
observed over BLE. Build success is not proof of calibrated accuracy, radio
reliability or battery life; verify the complete workflow on your hardware.

## Contributing

| Path | Purpose |
| --- | --- |
| [firmware](firmware/CMakeLists.txt) | V2 configuration and the shared main loop/compass maths |
| [firmware-v1](firmware-v1/CMakeLists.txt) | Early-v1 configuration and MAG3110 driver |
| [components/bthome](components/bthome/__init__.py) | ESPHome component and shared packet codec |
| [scripts/microbit.sh](scripts/microbit.sh) | Board/profile selection and guarded flashing |
| [tests](tests/protocol_test.cpp) | Protocol/compass and deployment-guard tests |

Run the tests above before submitting changes. For firmware changes, build both
board targets and both battery profiles. For receiver changes, validate and
compile the gateway using ESPHome 2026.10.0. Include the actual sensor chips and
hardware test results when proposing support for another board revision.

Bug reports should include board/sensor revision, target/profile, tool versions,
reproduction steps and relevant serial/ESPHome logs. Remove Wi-Fi passwords, API
keys, OTA credentials and other private information. Do not commit personal
deployment YAML or generated binaries.

## Licence

**A project licence has not yet been added.** Public repository access alone is
not an open-source licence; do not assume permission to modify or redistribute
the project until an explicit licence is supplied. Zephyr, ESPHome and their
dependencies retain their respective licences.

## References

- [BTHome V2 format](https://bthome.io/format/)
- [Zephyr 4.2 setup guide](https://docs.zephyrproject.org/4.2.0/develop/getting_started/index.html)
- [ESPHome documentation](https://esphome.io/)
- [micro:bit hardware documentation](https://tech.microbit.org/hardware/)