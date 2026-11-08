[简体中文](passport-app-requirements.zh_CN.md) · **English**

# AI Passport Application Development Requirements (Phase 1)

Date: 2026-11-06. Status: requirements based on the discussed UX, not evidence of implementation or device validation.

## Approved phase 3 extension: weather and network time

Add Weather View after Music Remote in the seven-entry device menu. Display
city, condition, temperature, feels-like temperature, humidity, wind speed,
and update time. OK requests a refresh; hold OK returns to the menu. Show cached
weather offline and an explicit empty state before the first successful update.
The card's weather panel shares the same city, condition, and temperature.

Settings retains the existing hotspot under Hotspot Switch/Information and adds
separate Wi-Fi Switch/Information entries for external networking. The eight
entries scroll in a six-row window. Keep the hotspot SSID and management address;
network selection and external Wi-Fi password entry take place only in the local
webpage at `http://192.168.4.1`. The two switches are independent. Hotspot starts
off; the external Wi-Fi preference and credentials survive reboot.

The webpage offers Shenzhen and Wuhan only, Shenzhen by default. Save the
weather interval of 30, 60, 120, or 240 minutes, defaulting to 30. Synchronize the
card clock once per boot when Internet time succeeds, then keep local time.
Show a marked cached time on an offline boot, or a waiting state without cache.
This supersedes earlier weather/time deferrals. See the
[application guide](../development/passport-card-remote.md) for validation.

## Approved phase 2 extension

The later approved Wi-Fi UX supersedes phase 1's deferred profile/photo import
scope below. Add the local **Passport WIFI** hotspot, Wi-Fi switch/information
page, and embedded management at `http://192.168.4.1`. Edit avatar, name,
constellation, zodiac and daily tag; manage at most six photos. Information
shows name, configured password and address even when Wi-Fi is off. Keep passwords
out of tracked requirements/source. Require profile preview, explicit save and
photo-delete confirmation. Fit JPG/PNG without cropping and retain saved content
across boots. Wi-Fi starts off each boot. Weather editing and real transit remain
deferred. See the [application guide](../development/passport-card-remote.md).
The remaining sections retain the original phase 1 baseline.

## 1. Goal and scope

Build a Chinese personal card, Android QQ Music remote, photo viewer and system settings on the default hardware. Boot directly into the card. Reuse the BSP and implement the discussed application UX rather than the hardware-test demo shell.

| Module | Phase 1 scope |
| --- | --- |
| Personal card | Replaceable bundled profile, daily tag and default weather; no details |
| Music remote | BLE media commands, connection state and phone volume steps |
| Photo viewer | Up to six bundled examples, manual selection and fullscreen |
| Settings | Display timeout, Bluetooth switch, pairing mode and clearing bonds |
| Metro/bus | Menu entries and UX placeholders only; real functionality deferred |
| Profile configuration/photo import | Deferred, including phone/computer companion tools and transfer interfaces |

Song metadata, live weather, photo autoplay and high-speed rail codes are deferred. No import protocol or transit integration design is required. Do not add fake import controls.

## 2. Pages and shared input

Use the 240 by 320 portrait display and Up, OK and Down buttons. Power is independent hardware.

- Menu order: personal card, music remote, photos, metro code, bus code, settings.
- Up/Down select with wraparound; OK enters. Hold OK in the menu returns to the card.
- Hold OK on feature pages returns to the menu. Settings subpages return to the settings list; the list returns to the menu.
- Use the UX's 0.5-second hold threshold as the default. One press must not generate both short and long actions.
- No Up/Down hold shortcuts except music volume.

## 3. Personal card

- Show date, weekday, battery, default avatar, example name, constellation and Chinese zodiac as specified in the Chinese version.
- Split the upper card approximately 60:40: widen the avatar and move name/constellation/zodiac to the right, retaining a gutter. Preserve date, battery and the lower daily panel.
- Up/Down switch between daily tag and daily weather.
- Use the confirmed default curiosity/take-it-slowly tag and fixed Shenzhen example weather labeled as default, never live.
- Short OK does nothing extra; no detail page.
- Centralize replaceable example content. Phone/computer profile, tag and weather configuration is deferred.
- Use available time/battery capabilities. Invalid values require clear placeholders, not examples presented as real readings. New clock-setting tools are deferred.

## 4. Music remote

Use ESP32-C3-compatible BLE media control for Android, primarily QQ Music acceptance. Audio plays on the phone.

| Input | Action |
| --- | --- |
| Short Up | Previous track |
| Short Down | Next track |
| Short OK | Play/pause |
| Hold Up | Volume up one phone system step, repeated every three seconds |
| Hold Down | Volume down one phone system step, repeated every three seconds |
| Hold OK | Return to menu |

- Send the first volume command at the hold threshold and repeat every three seconds from there. Release stops immediately without queued or catch-up commands.
- The phone defines the step size. Do not promise five percent or show an unsynchronized exact percentage.
- Keep the fixed song-information-pending text from the Chinese requirements. Title, artist, artwork and progress synchronization are deferred.
- Show only battery at the upper right, with a device name, song-information card and three-column media controls below.
- Show connection state inside the middle song-information card. Disconnection stays on this page and shows disconnected status.
- Place an outlined circle above play/pause, aligned with previous/next icons. Retain volume rules, pairing instructions and the hold-OK menu hint below.
- Center the short-press track/hold-volume usage hint. Successfully queued previous/next commands show the corresponding track-action label centered instead of a generic sent message. Held volume still repeats every three seconds.
- Suppress commands while Bluetooth is off or disconnected and provide brief feedback.
- Without phone feedback, locally toggled icons must not claim confirmed phone playback state.
- Re-pair through settings, without hidden double-press shortcuts.

## 5. Photo viewer

- Support up to six bundled example images.
- Up/Down select with wraparound; OK enters fullscreen and OK again exits.
- Preserve the complete image and aspect ratio in both modes, adding letterboxing instead of cropping or stretching.
- Normal mode may show current/total. Fullscreen hides title, counter and hints while retaining navigation and hold-OK return.
- No filename, capture time, photo details or autoplay.
- An empty library shows an empty state; selection and OK cannot enter invalid states.
- Import, deletion, transfer, format conversion and phone/computer tools are deferred.

## 6. Transit codes: UX only

- Retain separate metro/bus entries and placeholders, with no high-speed rail.
- Clearly label the demonstration as unusable for travel. Include no real tickets or authorization data.
- Up/Down switch between placeholders. Short OK has no transaction action; hold OK returns to the menu.
- Keep these pages on and restore the user's timeout policy on leaving.
- Alipay integration, phone linkage, real codes, refresh, expiry, payment and import are deferred.

## 7. System settings

List display timeout, Bluetooth, pairing mode and clearing bonds in that order. Up/Down select; OK executes or opens a subpage.

### 7.1 Display timeout

- Options: 15, 30, 60 seconds and always on; factory default 15 seconds.
- Confirm applies and persists the value across boots. Hold OK cancels an unconfirmed selection and returns.
- Ordinary pages turn off after idle timeout; input resets the timer. Retain the page and BLE capability; display-off is not whole-device sleep.
- The first press while off only wakes. Consume that entire press/hold until release without navigation, volume, playback or menu actions.
- Wake restores the page and restarts timing. Transit placeholders bypass automatic display-off.

### 7.2 Bluetooth and pairing

- Off disconnects and stops connectable advertising without clearing bonds.
- Implementation default: enabled on first boot; persist the switch. Re-enabling permits bonded-phone reconnection.
- Pairing requested while off prompts the user to enable Bluetooth.
- Pairing mode permits discovery through Android system settings. Clearly indicate waiting for pairing if an existing connection must be disconnected.
- Show the device name and instructions without requiring deferred companion software. Successful connection returns to the settings list and updates status.
- Leaving the pairing page does not clear bonds. Validate reconnection policy and compatibility during implementation.
- Clearing requires confirmation with Cancel selected initially. Cancel preserves bonds; confirm disconnects and removes local bonds.
- Retain profile, photos, timeout and all other settings.
- If Android retains a stale bond, instruct the user to forget the device there and pair again.

## 8. Power: hardware validation prerequisite

- Target interaction: short Power toggles display-off/wake on any page; hold powers off/on.
- Firmware visibility of short Power is unverified. Inspect hardware/BSP and test on-device instead of assuming ordinary-button support.
- If unsupported, report the blocker and proposed alternative for confirmation. Do not silently repurpose existing gestures.
- Preserve existing hardware power behavior and timing.
- Transit always-on bypasses automatic timeout only. Explicit short Power takes priority if supported.

## 9. Implementation constraints

- Reuse BSP display, buttons, battery and backlight. Shared hardware support belongs in `components/bsp`; pages, state and BLE application logic belong in `main`.
- Validate Chinese glyph coverage, widget font selection and on-device rendering for missing or clipped text.
- Keep callbacks nonblocking, use workers for slow operations and lock cross-task LVGL access. Stop related tasks, timers and callbacks before destroying pages.
- Make navigation, press classification, repeat timing, wake consumption, timeout and persistence rules independently host-testable.
- Restore defaults for invalid/missing settings; avoid frequent Flash writes for idle timing.
- Add no import/transit protocols and commit no personal data, ticket secrets or unsanitized logs.

## 10. Acceptance and delivery

| Area | Pass condition |
| --- | --- |
| Boot/navigation | Cold boot opens card; six entries and return paths work; no rail |
| Card | Tag/default weather switch correctly; no details; complete unclipped Chinese |
| Music | Previous/next/play-pause work on Android QQ Music; disconnect retains page |
| Volume | Initial step, three-second repeats, immediate release stop, no disconnected commands |
| Photos | Zero/one/six boundaries, wraparound, fullscreen and letterboxing work |
| Display | Four choices, 15-second default, boot persistence and complete wake-press consumption |
| Bluetooth | Off/on, pairing, reconnection and clear/cancel work; clearing affects only bonds |
| Transit UX | Clearly unusable for travel, stays on, restores timeout on leaving |
| Power | Separate short-Power hardware and long-power results; simulation is not validation |

Firmware delivery must run static checks, host tests, build and the complete gate, reporting Build, Host tests, Device tests and Unverified separately. Supply a verified merged image flashable at `0x0` and matching debugging artifacts. Proactively offer device tests and obtain authorization before flashing.

This document defines requirements. See the [application guide](../development/passport-card-remote.md) for implementation and validation records. Earlier application tests do not establish acceptance of this specification.
