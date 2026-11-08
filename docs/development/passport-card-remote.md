[简体中文](passport-card-remote.zh_CN.md) · **English**

# Personal Card and Android Media Remote

## Weather request memory fix (2026-11-07)

On the first weather firmware, the user confirmed the menu, retained files,
Hotspot setup and external Wi-Fi association. The weather page stayed empty
and reported low memory. COM3 reported 23,108 bytes of free heap and a
14,336-byte largest block on the weather page. That block is smaller than the
original 24,576-byte preflight requirement for opening HTTPS.

The fix uses an 8 KiB TLS receive payload and sizes the preflight check from
that setting with a 1 KiB allowance. TLS certificate validation stays enabled.
The client sizes a known-length response buffer to the response and releases
the TLS connection before parsing JSON. Both public city requests returned HTTP
200 in a host TLS 1.2 probe, with maximum incoming records of 3,410 bytes.
The LVGL regression check confirms that an associated device with no weather
sample displays the connected status even when a request is deferred.

The complete validation gate and archive verification passed. The repaired
application is 3,087,680 bytes and the partition table matches the flashed
weather build. The authorized COM3 write at `0x10000` passed hash verification.
Startup matched ELF prefix `3d3a07509`, and the saved station connection acquired
an IP. The subsequent station-only sample reported 40,696 free heap bytes,
a 27,648-byte largest block and a 9,124-byte boot minimum. The user confirmed
normal weather display after requesting a refresh with Hotspot and external
Wi-Fi both active. This passes the functional retest for the reported failure.

- Merged image SHA-256 `33c62c39aec6db7f4f5ef6117a0280d1a4a4fcb33533ea49ecb415c49b02a958`
- ELF SHA-256 `3d3a075094be1f89846a64a57d8814d6dc80f2cec0a440fc1a6642c4205dce20`

## Weather, external Wi-Fi and network time (2026-11-07)

This version implements the approved seven-entry menu and eight settings rows
in a six-row scrolling window. Hotspot is the existing management AP, off at
boot; Wi-Fi is a separate station connection that remembers its switch and
credentials. Keep the existing SSID and `192.168.4.1`. The embedded Passport
Hotspot page adds Network Settings beside the existing profile/photos tabs:
scan 2.4 GHz networks, choose an SSID, enter its password and save/connect.
The device displays connection state and station IP without a password input.
SSID characters outside the fixed device font are marked with `?`; the full
name is available on the webpage. Management requests must target the Hotspot
socket address as well as pass the existing Host/Origin/token checks.

The weather worker requests the [Open-Meteo current conditions](https://open-meteo.com/en/docs)
over certificate-verified HTTPS and labels them as modeled weather. Select only
Shenzhen or Wuhan and persist a 30/60/120/240-minute interval (default 30).
Weather View and the card share the same city/condition/temperature snapshot.
The detailed page adds feels-like temperature, humidity, wind and update time.
OK requests a refresh; a one-minute minimum request interval and bounded retry
backoff protect the service. Each city retains its own last valid cache. The
first successful SNTP sync per boot sets local time (UTC+8) and saves an epoch;
offline boots show cached time as such, or a waiting state with no cache. A
cache cannot measure elapsed time while power was off. Weather/UI work stays
outside button callbacks. Scans, binary uploads and weather TLS serialize
their largest allocations; requests defer when memory is busy.

The partition table is byte-identical to the prior accepted Wi-Fi firmware.
Existing profile, photos, Bluetooth and display settings use unchanged formats.
New settings are stored in separate NVS namespaces. An application-only update
at `0x10000` is appropriate on the prior verified layout to preserve data;
the merged `0x0` image can reset NVS and its data selectors.

- Build: **PASS**, complete `./tools/validate.sh` with ESP-IDF 5.5.3 and
  independent archive verification. Application **3,087,488 / 4,128,768 bytes**.
- Host tests: **PASS**, 17 C programs, 111 Python cases, 361 fixed glyphs at
  12/16/20 px, and the separate Node webpage test. Also checked the actual
  JSON parser with the pinned cJSON, public response field/unit/size, 320 px
  webpage layout with mock responses, and actual LVGL screen label bounds.
  The 24 KiB LVGL host pool peaked at 18,584 payload bytes over 100 transitions.
- Device tests: **PASS for flashing and startup**. With user authorization,
  esptool verified the application-only write at `0x10000` on COM3. The
  partition table and NVS were outside the write range. Startup matched ELF
  prefix `99eb4d638`, recognized `passport_data`, and showed no panic,
  watchdog, missing glyph or service initialization failure in 18 seconds.
  Free heap at 1.498 s was 73,916 bytes, with a 65,536-byte largest block.
- Unverified: live AP/STA scanning, provisioning and reconnect, SNTP/TLS from
  the board, cache across power-off, Wi-Fi/BLE coexistence and peak heap/power.

Merged image: `build/FoloToy-AI-Passport-full.bin`, **3,153,024 bytes**, at **0x0**.
Merged SHA-256: `95edd90157920f4b15eda901f4be7fead52def4527451732eda3461ef3c557c2`.
ELF SHA-256: `99eb4d6380a0bb5189ad7360366ec8bcc0def1f75267b7bd2f9ab57d34ae9e71`.
Archive: `build/firmware/95edd90157920f4b15eda901f4be7fead52def4527451732eda3461ef3c557c2/`.
Local gate: `build/ux/weather-gate.log`. Device logs: `build/device-test/weather-flash.log`
and `build/device-test/weather-startup.log`.

## Local Wi-Fi management (2026-11-06)

Settings now has six entries: timeout, Wi-Fi, Wi-Fi information, Bluetooth,
pairing and clear bonds. Wi-Fi starts off each boot. Enable it, connect a phone
or computer to **Passport WIFI**, then open `http://192.168.4.1`. Wi-Fi information
shows the SSID, deployment password and address even when the hotspot is off.
This supersedes the earlier deferred companion-tool scope below.

The embedded, offline Chinese webpage edits avatar, name, constellation, zodiac
and daily tag with preview and explicit save. Names allow four CJK or eight
ASCII characters, sharing that budget for mixed input; tags allow sixteen
characters. JPG/PNG input is limited to 5 MiB per file. Manage up to six photos,
replace them or delete with confirmation. The original six samples remain until
replaced/deleted. Images fit fully without cropping and OK still toggles device
fullscreen. Weather editing, music metadata and real transit integration remain
deferred.

Dynamic profile text is rasterized using the browser's installed font. Preview
must be checked for missing characters before saving. The device does not rely
on its fixed menu subset for arbitrary names. Names use 20 px, or 18 px across
at most two lines for wide text; tags use 14 px across at most two lines. Avatar
and rendered text persist together. Fixed menus cover 312 glyphs at 12/16/20 px.
Fixed menus and the user's uploaded profile text/avatar were accepted on the
device on 2026-11-07.

The factory partition is now `0x10000..0x3fffff` (size `0x3f0000`), with a 4 MiB
`passport_data` partition at `0x400000`. NVS/PHY offsets remain unchanged. Pixel
data is mapped from Flash without whole-image RAM allocations. Two banks per
object and one committed NVS selector preserve old data on failed uploads.
The HTTP worker uses 2 KiB chunks, bounded dimensions/lengths and streaming CRC;
publication and LVGL cache/object invalidation hold the BSP lock. Clearing bonds
retains all profile/photo/settings namespaces.

Supply the deployment password through `PASSPORT_WIFI_PASSWORD` or the ignored
local `build/tooling/passport_wifi_password.txt` (8–63 ASCII characters from the
supported letters/digits/punctuation set). It is embedded in local firmware and
shown on the device, but absent from tracked source, logs and the web state API.
The webpage uses no Internet assets, cloud or account login. Writes require a
per-boot request token and local Host/Origin. Hotspot access is protected by its
password; local HTTP itself is unencrypted.

First upgrade requires the new partition table together with the application.
Flashing only the application against the old layout cannot enable storage.
Segmented partition-table/application flashing can retain existing NVS/bonds;
no whole-chip erase is required. Merged `0x0` flashing can reset NVS/file selectors
and restore visible defaults even when resource bytes remain. It does not
guarantee user-data preservation.

Host checks cover protocol/model logic and the actual asset worker: short reads,
interruptions, CRC, Flash/NVS failure, deletion/replacement and restart. Run the
additional webpage packing/limits/draft-recovery test with
`node tests/test_passport_web.cjs`. The actual webpage was inspected at 320 px
with mock responses; this does not establish on-device connectivity.
Build: **PASS**, complete `./tools/validate.sh` with ESP-IDF 5.5.3 and independent
archive verification. Host tests: **PASS**, 16 C programs, 111 Python cases,
312-glyph font checks and the separate Node webpage test. Actual LVGL renders
passed label/bounds checks for both Wi-Fi information states and the six-row
list; 100 transitions peaked at 18,592 payload bytes in the 24 KiB host pool.
The final application contains the exact current embedded webpage bytes.
Device tests: **PASS for this increment's functional acceptance**, including
authorized flashing/startup and the user-confirmed checks below. COM3 identified
the ESP32-C3.
The verified table at `0x8000` and **2,746,336-byte** application at `0x10000`
were written with esptool 4.13.0; both data hashes passed. NVS/PHY entries match
the previous layout and neither NVS nor the resource partition was written.
No full-chip erase was performed. Startup matched ELF prefix `de4c39a64`,
recognized the new resource partition and showed no panic, watchdog, missing
glyph, file-storage or Wi-Fi-service initialization error. At 1.121 s the card's
LVGL free/largest/peak payload was 12,980/12,728/10,556 bytes; system
free/largest/minimum heap was 85,276/73,728/85,276 bytes with Wi-Fi off.
A transient advertising error 6 was followed by a BLE notification at 8.528 s.
The bounded startup monitor closed. Local logs: `build/device-test/wifi-flash.log`
and `build/device-test/wifi-startup.log`. These checks do not establish hotspot
client connectivity, upload correctness or Wi-Fi-on memory headroom.
The additional 45-second observation recorded SoftAP startup and DHCP at
`192.168.4.1`, without crash/watchdog markers. The monitor closed; local log:
`build/device-test/wifi-interaction.log`. On 2026-11-07 the user confirmed normal
card startup, all six settings entries, correct Wi-Fi information with complete
Chinese text, phone hotspot connection and browser access, and successful loading
of profile data and all six bundled photos. The user subsequently confirmed:

- Name, constellation, zodiac, daily tag and avatar preview/save update the
  device correctly, with complete Chinese text and correct avatar proportions.
- Photo replacement, deletion and addition work; device navigation and OK
  fullscreen entry/exit preserve complete images without distortion.
- Power off/on retains profile and photos, starts Wi-Fi off, and re-enabling
  Wi-Fi reloads the saved content in the webpage. QQ Music remote control works.

No private profile values or uploaded images were collected into this record.

Verified merged image: `build/FoloToy-AI-Passport-full.bin`, **2,811,872 bytes**,
for **0x0**. Application: **2,746,336 / 4,128,768 bytes**.
Merged SHA-256:
`346f4049876d87d3e9152560f3055191da4a8ab52f320e9f5ae83bdb41edfebb`.
Matching ELF SHA-256:
`de4c39a6452ab8feb68e052bb8239775f371fe78aa1186b6b5e3688a481da847`.
Archive: `build/firmware/346f4049876d87d3e9152560f3055191da4a8ab52f320e9f5ae83bdb41edfebb/`.
Local gate/render logs: `build/ux/wifi-complete-gate.log` and
`build/ux/wifi-lvgl-render.log`. No rebuild, commit or push occurred during testing.
Unverified: desktop-browser uploads, repeated-cycle/long-duration stability,
power loss during an active write, simultaneous upload and BLE control under
load, Wi-Fi-on peak heap and battery consumption. Normal phone uploads, restart
persistence and music control passed the user checks above; those checks do not
establish stress-test results. Flashing above was explicitly authorized;
additional firmware versions require their own approval.

## Wider personal-card avatar (2026-11-06)

The approved upper-card layout uses an approximately 60:40 avatar/profile split.
After a 10 px gutter, the 208 px content width becomes a 118 px avatar and an
80 px profile column starting at x=144. Name, constellation and zodiac move
right; the placeholder and caption are centered in the wider avatar. Existing
20 px name text is retained and smaller captions use the available 12 px font.
The date, battery, daily panel, navigation and other pages remain unchanged.
Device acceptance below belongs to earlier images. Validation for this update:

- Build: **PASS**, complete `./tools/validate.sh` gate with ESP-IDF v5.5.3,
  merged-layout validation and independent debug-archive verification.
- Host tests: **PASS**, 14 C programs, 111 Python tests and existing 299-glyph
  coverage at 12/16/20 px. Actual LVGL checks confirmed single-line, in-bounds
  card labels in both tag/weather states; the renders were visually inspected.
  After 100 page transitions, host peak payload was 18,592 bytes in the 24 KB
  pool. Card free/largest block was 6,384/5,888 bytes.
- Device tests: **PASS (this update's functional acceptance)** after authorized
  application-only flashing on `COM3` and user confirmation.
- Unverified: existing long-term Bluetooth/power and short-Power hardware limits.

Merged image: `build/FoloToy-AI-Passport-full.bin`, **2,242,224 bytes**, **0x0**.
Application: **2,176,688 / 8,323,072 bytes**; partition layout unchanged.
Merged SHA-256:
`96461c886f1964b9ffdbdd91dd43766d45a43e79d4d0ce25a7fdba9c96ceeda1`.
Matching ELF SHA-256:
`20ee95e482932cadabff0bba869e6ebca545684e86c5778fb1dfb752ead7dc38`.
Verified archive:
`build/firmware/96461c886f1964b9ffdbdd91dd43766d45a43e79d4d0ce25a7fdba9c96ceeda1/`.
Local logs: `build/tooling/card-wide-complete-gate.log` and
`build/tooling/card-wide-render.log`; rendered card: `build/ux/native/card.png`.
No commit or push was performed. Merged flashing may reset settings; this test
instead used compatible application-only flashing with NVS untouched.

The user authorized flashing. The new/previous partition tables matched, and
the verified **2,176,688-byte** application was written at **0x10000** on `COM3`.
Application SHA-256:
`62005dbf24447dacf38c5f5c7a1df701c04d67fdd07e53585d649bbc969a58ef`.
esptool 4.13.0 data-hash verification passed without whole-chip erase. Startup
matched ELF prefix `20ee95e48`, with no glyph failure, panic or watchdog observed.
Card LVGL free/largest/peak payload at 1.031 seconds was 12,968/12,716/10,568
bytes; system free/largest/minimum was 138,184/114,688/138,184 bytes. One
`Advertise: 6` at 5.091 seconds was followed by a notification at 5.473 seconds.
The user confirmed the wider avatar, right-side profile proportions, complete
text, date/battery and tag/weather switching all match expectations. The bounded
monitor closed. Local logs: `build/device-test/card-wide-flash.log` and
`build/device-test/card-wide-startup.log`. No rebuild occurred during testing.

## Music hint and track feedback update (2026-11-06)

The approved UX now uses a centered short-press track/hold-volume usage hint.
Successfully queued previous/next commands display their corresponding action
labels instead of the generic sent message. Volume behavior, play/pause feedback,
connection handling and the circle layout remain unchanged. The fixed-font
inventory is regenerated for the revised copy. Earlier device results below
belong to previous images.

- Build: **PASS**, complete gate with ESP-IDF v5.5.3, layout validation and
  separate debug-archive verification.
- Host tests: **PASS**, 14 C programs, 111 Python tests and 299 glyphs at each
  of 12/16/20 px. Actual LVGL renders of the normal/previous/next feedback states
  were checked for centered, single-line text without clipping; 100 page
  transitions passed with a reported 18,592-byte peak payload in the 24 KB host pool.
- Device tests: **PASS (this update's functional acceptance)** after authorized
  flashing on `COM3` and user confirmation of the revised hint and track feedback.
- Unverified: extended Bluetooth/power measurements and the existing short-Power
  hardware limitation; these were not part of this copy-change acceptance.

Previous merged image: **2,242,208 bytes**, **0x0**, retained in its verified
archive below. The shared delivery filename now contains the wider-card update.
Application: **2,176,672 / 8,323,072 bytes**; partition layout unchanged.
Merged SHA-256:
`fb8e9e31b888ffb081ec55f9d75ef544e3c41e8f4dff043133f477fc65c77ce5`.
Matching ELF SHA-256:
`2695812e71965a4674fbc581d177612aa92c8ca3bea58394ff31d38ee195a0aa`.
Verified archive:
`build/firmware/fb8e9e31b888ffb081ec55f9d75ef544e3c41e8f4dff043133f477fc65c77ce5/`.
Local logs: `build/tooling/music-feedback-complete-gate.log` and
`build/tooling/music-feedback-render.log`; render comparison:
`build/ux/native/music-feedback-sheet.png`. No commit or push was performed.
Merged flashing may reset stored settings; the authorized test below instead
used compatible application-only flashing to leave NVS untouched.

### Authorized feedback device test

The user explicitly requested testing. The sole Espressif device was `COM3`.
The new and previously installed partition tables matched byte-for-byte; the
selected application SHA-256 was verified against its manifest:
`0597b0fcbdc5be0beb5ca0250106db14bd1f14322a44ae3389aeff99f13b7d60`.
Only **2,176,672 bytes at 0x10000** were written. esptool 4.13.0 data-hash
verification passed, without whole-chip erase or NVS/PHY/partition/bootloader writes.
One reset and bounded startup capture matched ELF prefix `2695812e7` and
ESP-IDF v5.5.3; no glyph-check failure, panic or watchdog was observed.
At 60.898 seconds on the asleep card page, LVGL free/largest/peak payload was
12,976/12,724/10,548 bytes and system free/largest/minimum heap was
138,184/114,688/138,184 bytes. The user confirmed that the revised usage hint
is centered, Up/Down display the corresponding previous/next labels centered,
and actual QQ Music track switching works. All requested acceptance items for
this update were reported as matching expectations.
Local logs: `build/device-test/music-feedback-flash.log`,
`music-feedback-startup.log` and `music-feedback-interaction.log` in that directory.
Both bounded monitors closed and released the port; the firmware was not rebuilt.

## Music page revision (2026-11-06)

The music screen follows the approved card layout: device name, fixed pending
song-information heading, connection status inside the card, and three columns
for previous/play-pause/next. Play/pause has an outlined circle above its label.
Only battery appears at the upper right. Volume repetition, pairing instructions
and the hold-OK menu hint remain visible below. Icons are LVGL lines/objects;
they do not depend on missing font symbols. A 12 px Chinese subset supplements
the 16/20 px fonts for compact labels. No media-command or BLE behavior changes.

Validation for this revision:

- Build: **PASS**, complete `./tools/validate.sh` gate, ESP-IDF v5.5.3,
  merged-layout checks and a separately verified matching debug archive.
- Host tests: **PASS**, 14 C programs, 111 Python tests and 299 glyphs at each
  of 12/16/20 px, including the missing-glyph negative probe.
- Actual LVGL rendering: **PASS**, connected, disconnected, connecting,
  Bluetooth-off and failed states, including unknown/100% battery. Music labels
  remained single-line and inside the display. All pages survived 100 transitions
  in the 24 KB host pool; reported peak payload was 18,592 bytes, with at least
  4,712 free bytes in the captured music states. The rendering was visually
  checked for battery-only header, card status and circle/skip icon alignment.
- Device tests: **PASS (listed checks below)** after authorized application-only
  flashing and user acceptance of the revised screen and controls.
- Unverified: every physical error-state layout, extended Bluetooth endurance
  and power consumption. Short Power remains deferred.

Previous merged image: **0x0**, **2,242,176 bytes**, retained in its archive
below. The shared delivery filename now contains the newer feedback update.
The application is **2,176,640 / 8,323,072 bytes** in the unchanged factory partition.
Merged SHA-256:
`adeca6a5b2d06a890292066e5b7a6b960d161a16561662ee33bc62e0483045bd`.
Matching ELF SHA-256:
`7ffd24b5cc266cf65eb4126cd10451a7a7e279a9452dadd0bf0d6a5bea6ec28d`.
Verified archive:
`build/firmware/adeca6a5b2d06a890292066e5b7a6b960d161a16561662ee33bc62e0483045bd/`.
Local logs: `build/tooling/music-ui-complete-gate.log` and
`build/tooling/music-ui-render.log`. Rendered music screen:
`build/ux/native/music.png`. No commit or push was performed.
Merged flashing may reset bonds/settings; the authorized test below instead
updated only the compatible application partition.

### Authorized music-layout device acceptance

The user requested flashing/testing of this revision. The sole Espressif
USB Serial/JTAG device was `COM3`, the same ESP32-C3/8 MB board. Before writing,
the archived new/previous partition tables were compared byte-for-byte and the
application hash was verified against the manifest. Only the **2,176,640-byte**
application was written at **0x10000**; bootloader, partition table, NVS and PHY
regions were left untouched. Application SHA-256:
`47cfd390de9728e858396a044625f9e18a609ff9a18100bf0881a8161186d3da`.
esptool 4.13.0 data-hash verification passed; no whole-chip erase was performed.

A reset and bounded startup capture matched ELF prefix `7ffd24b5c` and ESP-IDF
v5.5.3. Display, buttons, battery and BLE initialized, without glyph-check
failure, panic or watchdog observed. One transient `Advertise: 6` warning at
5.413 seconds was followed by a notification at 5.776 seconds; it did not recur
in the subsequent interaction capture. At 60.901 seconds on the music page,
LVGL free/largest/peak payload was **11,992/11,760/11,476 bytes**; system
free/largest/minimum heap was **138,184/114,688/138,160 bytes**. Captured media
notification press/release pairs were about 62 ms apart.

The user confirmed the battery-only upper right, connection status inside the
song card, circle above play/pause, and complete unclipped Chinese text. QQ
Music previous/next, play/pause and held three-second volume steps work. Phone
disconnection retains the music page and updates the card. Previous Bluetooth
bonds and display-timeout settings were retained. These are functional checks,
not a continuous endurance or power measurement.

Local logs: `build/device-test/music-ui-flash.log`, `music-ui-startup.log` and
`music-ui-interaction.log` in that directory. All bounded monitors closed and
released the port. No firmware was rebuilt during the flash/test procedure.

## Current phase 1 application

The [phase 1 requirements](../assets/passport-app-requirements.md) supersede the
historical two-page prototype below. `main/main.c` starts the personal card with
a six-entry menu, music remote, photos, metro/bus UX placeholders and settings.
Hold OK opens/returns to the menu; settings subpages return to the list first;
hold OK in the menu returns to the card.

The card has tag/default-weather panels and no details. Six original sample
scenes are bundled in Flash. Photo OK toggles aspect-preserving fullscreen;
Up/Down select manually. Profile configuration, photo import, companion tools,
song metadata, live weather and real transit functionality are deferred.
Transit demonstrations are marked unusable for travel and bypass auto timeout.

Music short Up/Down select previous/next and OK sends play/pause. Hold Up/Down
sends one phone system volume step at 500 ms and every three seconds thereafter
until release, without a five-percent guarantee. Song information stays pending;
disconnection retains the page. Delivery to BLE is not acknowledgment by QQ Music.

Settings provide 15/30/60 seconds/always-on (default 15), Bluetooth on/off,
pairing and confirmation before clearing bonds. Timeout and Bluetooth choice
persist in the `passport_app` NVS namespace; invalid values restore defaults.
NVS is never automatically erased. Display-off disables backlight while keeping
BLE and page state. The entire first wake press, including a hold, is consumed.
Startup-held keys and lost input events require a physical release to recover.

To pair, enable Bluetooth and enter pairing mode, then select **Passport Remote**
in Android Bluetooth settings. Normal mode accepts bonded identities only.
Leaving the pairing page keeps pairing available until connection succeeds or
Bluetooth is switched off. Clearing affects NimBLE bonds only. Forget stale
Android bonds and re-pair if needed, especially because this HID descriptor adds
volume usages `0xE9`/`0xEA`. Bluetooth off disconnects and stops advertising; the
host remains allocated for re-enabling. This is not a measured low-power shutdown.

Power long-press retains hardware behavior. The BSP exposes no short-Power
signal, so Power display-off/wake is **not implemented**, pending hardware
verification. No substitute three-button gesture is added.

Current text uses regenerated 12/16/20 px Noto Sans SC subsets. Generate original
photo examples with `python tools/passport_photos.py` in the font tool's Pillow
environment. RGB565 normal/fullscreen variants are pre-sized in Flash, avoiding
runtime transform buffers. `tests/test_passport_model.c` covers navigation,
timeout, wake consumption, repeat timing, settings encoding and photo geometry.

### Previous phase 1 build and acceptance

- Build: **PASS**, ESP-IDF v5.5.3; the complete `./tools/validate.sh` gate passed.
  WSL2 runs host checks as `passport-tests`; an ignored local adapter delegates
  IDF commands to the existing Windows toolchain and copies regular artifacts
  back for unchanged layout/archive checks. The gate's final copy uses Python
  instead of chmod, which the Windows-mounted checkout does not support.
- Host tests: **PASS**, 14 C programs and 111 Python tests, repository checks,
  actionlint, and 295 glyphs at both sizes, regular weight 400.
- Additional UI check: **PASS**, actual LVGL 9.5.0 headless rendering of all
  pages and settings subpages, plus 100 transitions. With a 24 KB pool, reported
  maximum payload allocation was 18,600 bytes. The metro page had 2,696 bytes
  free and a 2,376-byte largest free block; the music page had the same 9,056
  bytes free before/after the transition run. These are 64-bit host results,
  not measurements of the board. Chinese layout and photo letterboxing were
  inspected from rendered images.
- Device tests: **PASS (listed functional checks below)** after explicit
  authorization and user observation.
- Unverified: extended Bluetooth reconnection endurance, explicit Bluetooth
  switch acceptance, every error-state layout, ten-minute heap/endurance
  measurement and power consumption. Short Power remains unimplemented and
  requires hardware verification. Long Power was exercised during persistence
  acceptance.

Previous merged image: **2,219,152 bytes**, **0x0**, retained in the verified
content-addressed archive below; the shared delivery filename now contains the newer revision.
Application: **2,153,616 / 8,323,072 bytes** in the unchanged factory partition.
Full SHA-256:
`467117acb6db74442bde8bddcd4235dbd5d278bad3998c78534f181a0be74ceb`.
Matching ELF SHA-256:
`f4b4e8f31dd59dfe2dcfad61ea6dafa8a1f976ee72c319e210a42a26f4cb108a`.
Verified bundle:
`build/firmware/467117acb6db74442bde8bddcd4235dbd5d278bad3998c78534f181a0be74ceb/`.
Its manifest binds the merged/application/bootloader/partition images, ELF, MAP
and flash arguments. Version: `1802df5-dirty`, branch: `feature/passport-app`.
No commit or push was performed. Merged flashing can reset NVS, including
bonds and settings; authorization is required before writing the device.

Local logs: `build/tooling/redesign-complete-delivery.log` and
`build/tooling/redesign-ui-final.log`; rendered pages:
`build/ux/native/contact-sheet.png`. Prototype device results below do not
establish acceptance of this version.

### Authorized phase 1 device test (2026-11-06)

After the user requested flashing and testing, the sole Espressif USB Serial/JTAG
device was identified on `COM3`: ESP32-C3 revision v1.1, 8 MB Flash. The exact
merged image above was written at `0x0`; esptool 4.13.0 verified the data hash.
No whole-chip erase was performed. A single reset and bounded startup capture
matched ELF prefix `f4b4e8f31` and ESP-IDF v5.5.3. Display, LVGL, buttons, battery
and BLE initialized, with no font self-check failure, panic or watchdog observed.
Battery reported 27% and 3913 mV, without a calibration claim.

At uptime 1.021 seconds, system free/largest/minimum heap was
138,316/114,688/138,316 bytes. LVGL reported 12,976 bytes free, a 12,724-byte
largest block and a 10,548-byte peak payload allocation on the card page.

The capture also showed repeated connection termination, HCI
`BLE_ERR_CMD_DISALLOWED` and intermittent `Advertise: 6` warnings. Old phone
binding/cache is a possible cause, not established by these logs. The user was
instructed to forget the old Android bond and enter pairing mode. Pairing-mode
advertising (`disc_mode=2`) appeared at uptime 91.199 seconds. The user later
confirmed phone control after re-pairing. The startup warnings remain recorded;
startup is not described as warning-free.

A subsequent bounded capture observed four notification press/release pairs
between uptime 144.456 and 165.317 seconds, with approximately 62 ms between
press and release. This establishes encrypted/subscribed HID activity, not QQ
Music execution. No earlier connection/advertising warnings appeared in that
capture. A later monitor attempt found `COM3` absent; it did not open or reset
another port. Further instrumented endurance measurements remain pending.

The user confirmed the following acceptance results:

- Chinese card and six-entry menu render correctly, without missing glyph boxes
  or clipping; hold-OK navigation works.
- Default 15-second timeout and wake-only first press behave as required.
- Photo navigation, fullscreen entry/exit and complete aspect-preserving display work.
- Android QQ Music previous/next, play/pause and held volume control all work,
  including three-second repeats and stopping on release.
- A 30-second timeout remains selected after long-Power off/on and takes effect.
- Canceling clear-bonds preserves control; confirming disconnects, preserves the
  profile, six photos and timeout, and permits pairing again.
- Transit demonstrations stay on beyond 30 seconds; returning to the card
  restores automatic display-off.

These observations establish the listed functional checks, not calibrated power
or a continuous ten-minute instrumented endurance run.

Raw bounded logs are local under ignored `build/device-test/redesign-*.log`;
each monitor closes on completion and releases the serial port.

## Earlier two-page prototype (historical)

An application for the default AI Passport hardware, with a new dark personal
card, mint accents, and battery at top right. The baseline test menu is not
reachable. Bluetooth remains active on both pages. The initial profile contains
sample information.

| Page | UP / DOWN short press | OK short press | OK long press |
| --- | --- | --- | --- |
| Personal card | Flip contact/about card | Flip contact/about card | Open media remote |
| Media remote | Previous / next track | Play/pause key | Return to personal card |

Long press uses the BSP's 500 ms threshold. Holding a key at startup is ignored
until release or a three-second guard timeout. Double click has no mapping;
use separate short presses. Button callbacks only enqueue events.

## Android connection

Pair **Passport Remote** in Android Bluetooth settings, wait for the ready
message, and start music in a phone player before using the remote. Readiness
requires an encrypted link and input-report notification subscription.

BLE HID Consumer Control (`0x1812`) sends previous (`0xB6`), next (`0xB5`), and
play/pause (`0xCD`). No companion app is needed. There is no audio streaming,
track-title retrieval, playback-state retrieval, or player acknowledgment. Audio
uses the phone's selected speaker/headphones. A sent message means that the BLE
stack accepted a report, not that the player acted on it. Compatibility with the
intended Android version, player, lock screen, and media-session routing needs
physical testing.

Disconnecting resumes advertising. NVS retains up to three bonds, with one active
phone at a time; automatic reconnection depends on Android, and manual selection
is also available. Forgetting and pairing again replaces only the requesting
peer's stale bond. Pairing uses Just Works. Link/subscription changes invalidate
queued commands. Every accepted press is followed by a zero report after 60 ms;
failed releases are retried before the next press.

## Customization and fonts

Edit `PROFILE_*` in `main/passport_text.h`, regenerate the fonts, and rebuild.
There is no phone/device profile editor. Compiled profile information is visible
on screen and embedded in the image; review it before sharing firmware.

`assets/fonts/` contains Noto Sans SC, its SIL OFL 1.1 license, the two generated
font sources, and the inventory/checksum manifest. Generation uses **Pillow
11.3.0** and **fonttools 4.60.1** in Python:

```bash
python tools/passport_font.py --generate
python tools/passport_font.py --check
```

The plain 4bpp 16/20 px subsets cover printable ASCII and all compiled UI/profile
strings. Generation verifies source coverage and pixel nibble packing. The
dependency-free check verifies inventory, cmap entries, and artifact hashes.
Startup checks actual LVGL glyph lookup for both fonts, including a known-absent
negative probe. This is a fixed inventory, not arbitrary Chinese input support.
Recheck line width, wrapping, and on-device rendering after changing the profile.

## Build and acceptance

Use ESP-IDF **5.5.3**, `dependencies.lock`, and the default 8 MB NVS/PHY/factory
layout. Display, buttons, and battery use BSP APIs. Wi-Fi and audio are not
started. Battery failure displays `--%`; Bluetooth failure leaves the card usable.
NVS initialization errors never cause automatic erasure.

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The gate adds navigation, HID pulse timing, and font checks while retaining
baseline/BSP tests. Native Windows uses `.exe` test outputs and needs a compatible
host C compiler and Windows `ACTIONLINT_BIN`. Keep ESP tools at a short path:
GCC's intermediate C++ include paths can exceed Windows path limits. Local tools
are under `build/tooling`, with a `D:/ap-idf-tools` junction; these paths are not
firmware dependencies.

Deliver the verified merged `build/FoloToy-AI-Passport-full.bin` at **`0x0`**, with
its matching manifest/ELF/MAP archive. Merged flashing may reset NVS (including
bonds) and PHY data. No whole-chip erase is required. Obtain explicit approval
before flashing; compilation or USB detection is not consent.

Physical acceptance includes both card variants and pages; all Chinese/error
states, clipping and battery fallback; fresh pairing and media keys; repeated
presses and startup-held keys; phone Bluetooth off/on, accessory power cycling,
stored-bond reconnection, forgetting/re-pairing, and no stale commands. Run at
least ten minutes with radio/UI active, recording free/minimum/largest heap and
LVGL peak allocation. System-heap metrics are logged periodically; actual power
consumption remains unmeasured. Report Build, Host tests, Device tests, and
Unverified separately. Host checks cannot prove phone behavior or display output.

## Delivery validation (2026-11-06)

- Branch: `feature/passport-app`; application version: `1802df5-dirty`.
- Build: **PASS**, ESP-IDF v5.5.3. The isolated firmware gate and subsequent
  archive verification passed. Merged image: **1,073,376 bytes**, offset **0x0**.
- Host tests: **PASS** in WSL2 Ubuntu 24.04.5 LTS. The entire `--static` gate
  passed as the ordinary user `passport-tests`: 14 C test programs, all 111 Python
  tests (no skips), repository checks, actionlint 1.7.12, and font coverage
  (198 glyphs at both sizes). This includes BSP audio recovery and all four demo
  lifecycle suites previously blocked by Windows Application Control. Earlier
  native-Windows attempts were blocked; no application-control policy was changed.
- Device tests: **PASS (limited checks below)** after the user authorized flashing.
- Unverified: phone-player behavior, reconnect endurance, all error-state layouts,
  ten-minute resource endurance, and power consumption.

Merged SHA-256:
`eb707e86ddc117f46dfc917bcad2584a398f46e0c6d898543208929e49755407`.
Matching ELF SHA-256 (also embedded in the application descriptor):
`d9489fd1f6ef479e33820796431fda49b6a2dffa73ef3c669be3175dbae97396`.
The verified archive is
`build/firmware/eb707e86ddc117f46dfc917bcad2584a398f46e0c6d898543208929e49755407/`;
its `manifest.json` records all retained files, sizes, offsets, and hashes.

### Authorized on-device test

On 2026-11-06 the user connected the board and explicitly requested flashing.
The sole Espressif USB Serial/JTAG device was `COM3`. Read-only identification
confirmed ESP32-C3 revision v1.1 and 8 MB Flash. The exact merged image above was
written from `0x0`; esptool's data-hash verification passed. No whole-chip erase
was performed. A subsequent reset/log capture matched the application version,
ESP-IDF v5.5.3, and embedded ELF hash prefix `d9489fd1f`.

The display, LVGL, buttons, battery, and BLE initialized. Battery reported 70%
and 3941 mV. No font-coverage failure, panic, or watchdog appeared in the captured
windows. The user confirmed that Chinese display, card navigation, and long-OK
page switching worked normally. Seven captured media-report pairs were separated
by 61–62 ms. Notifications establish stack activity, not player execution.

At uptime 60.923 s, free heap was 141,272 bytes, largest block 114,688 bytes, and
minimum free heap 137,996 bytes. One transient `Advertise: 6` (NimBLE ENOMEM)
warning occurred at 15.241 s; a report notification followed at 19.381 s and the
next captured interaction window showed no repeat. Its long-term recurrence is
unverified. At uptime 181.152 s, the same three heap values were observed and no
new warning appeared in that capture. Phone playback-control confirmation is
still pending. Raw local logs
remain under ignored `build/device-test/`; monitors close after each bounded
capture and release the port.

### WSL host-test follow-up

The user authorized WSL installation on 2026-11-06, then restarted Windows when
requested by the installer. WSL 3.0.3 and Ubuntu 24.04.5 LTS now run successfully.
Host validation used GCC 13.3.0 and Python 3.12.3, without installing another
ESP-IDF or changing/flashing the delivered firmware. The full `--static` gate
passed; the firmware gate remains the earlier verified native-Windows build.

The unchanged actionlint helper in the Windows checkout had CRLF line endings;
a temporary LF view was used to download its official checksum-pinned release.
The verified Linux actionlint 1.7.12 is also installed at `/usr/local/bin/actionlint`
inside Ubuntu for subsequent runs. Windows security/proxy configuration was not
changed. WSL reports that the host's localhost proxy is not mirrored in NAT mode;
the package downloads and tests nevertheless completed successfully.

Local environment details, complete test output, and setup/test scripts are in
ignored `build/tooling/wsl-checkpoint.json`, `build/tooling/wsl-host-tests.log`,
`build/tooling/wsl-host-setup.sh`, and `build/tooling/wsl-host-test.sh`.
To repeat the host gate from PowerShell:

```powershell
wsl -d Ubuntu-24.04 -u passport-tests -- bash /mnt/d/codex-project/ai-passport/build/tooling/wsl-host-test.sh
```
