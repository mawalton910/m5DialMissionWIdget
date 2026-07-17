# Guru Games - Mission - POI Story System
**Current Version**: POIStory v26.4.1  
**Platform**: ESP32-S3 (Crabik SLot ESP32-S3) with M5Stack Dial  
**Maintained by**: mawalton910

A location-based mission tracking system built on the M5Stack Dial (ESP32-S3) for immersive story-driven gameplay in live-action role-playing events.

---

##  What It Does

### For Players
- **Story-Driven Missions**: Receive narrative missions with specific objectives
- **Location Tracking**: Visit real-world locations to complete mission objectives
- **Mission Categories**: Choose from Heist, Police, Hacking, Job, Vehicle, Drug, Medical, or Community missions
- **Dynamic Narratives**: Each mission generates unique storylines with category-specific themes
- **Progress Tracking**: Configurable timer with real-time countdown and location visit tracking
- **Respect Rewards**: Earn difficulty-based rewards based on locations completed
- **Audio Feedback**: Distinct buzzer tones for every event  badge scan, mission start, location visits, completion, timeout, errors, and more

### For Game Masters
- **Device Monitoring**: View device information, firmware version, and WiFi status
- **Mode Switching**: Toggle between Mission, Buy Station, and Relay modes
- **Mission Logs**: Review all mission events with timestamps
- **Remote Updates**: Push firmware updates to all devices simultaneously
- **Integrated Systems**: Buy Station for loot and Relay for badge management
- **Mission Scrubbing**: Force-end an active mission and put all player badges on cooldown

---

##  Hardware

- **M5Stack Dial** (ESP32-S3) with built-in RFID reader
- 240240 round touchscreen display
- Rotary encoder with button
- Built-in speaker (I2S)
- WiFi connectivity for server integration

---

##  How Players Use It

1. **Scan Your Badge**  System identifies you and checks mission history
2. **Select Mission Type**  Scan a mission card (prevents consecutive duplicate missions)
3. **Read Your Story**  Device generates a unique narrative with 4 location objectives
4. **Navigate the Story**  Use rotary dial to scroll through mission pages
5. **Visit Locations**  Travel to real-world sites and scan location RFID tags
6. **Track Progress**  Press button to view countdown timer during mission
7. **Complete Mission**  Scan completion card when objectives are done
8. **Earn Respect**  Server awards difficulty-based rewards automatically

---

##  Mission System

### Mission Categories
| Category | Theme |
|---|---|
| Heist | High-stakes robbery operations |
| Police | Law enforcement missions |
| Hacking | Digital infiltration tasks |
| Job | Professional assignments |
| Vehicle | Transportation-based missions |
| Drug | Pharmaceutical operations |
| Medical | Healthcare-related tasks |
| Community | Social service missions |

### Difficulty & Rewards
Difficulty is earned based on locations visited:

| Locations Visited | Difficulty |
|---|---|
| 1 | Easy |
| 2 | Medium |
| 3 | Hard |
| 4 | Extreme |

Respect rewards scale with difficulty and are distributed automatically by the server.

### Mission Timer
- **Configurable Countdown**: Set in `Config.h` via `MISSION_TIMEOUT_MIN` (default: 15 minutes)
- **On-Demand Display**: Press BtnA during a mission to see MM:SS remaining
- **Warning System**: Red "HURRY!" alert when under 60 seconds
- **Timeout Handling**:
  - No progress: Mission holds on a timeout screen; cleared by badge-reset or completion card scan
  - Some progress: Locks current difficulty level, screen shows "SCAN COMPLETION CARD"

---

##  Audio System

All buzzer tones are defined in `Sounds.h` as `Note[]` arrays  edit that file only.

| Sound | Trigger |
|---|---|
| `SND_BADGE_SCAN` | Badge scanned / player added |
| `SND_BADGE_REMOVED` | Player de-registered |
| `SND_MISSION_START` | Mission card accepted |
| `SND_LOCATION_VISIT` | Location tag scanned |
| `SND_MISSION_COMPLETE` | Mission pass fanfare |
| `SND_TIMEOUT` | Mission timer expired |
| `SND_ERROR` | Invalid scan / general error |
| `SND_MISSION_SCRUBBED` | GM scrubs an active mission |
| `SND_BADGE_COOLDOWN` | Blocked or scrubbed badge scanned |
| `SND_RESET` | Badge reset / full reset |
| `SND_SENDING_DOTS` | Ascending blips during "Sending..." animation |
| `SND_SERVER_HOLD` | Hold tone while waiting for primary server response |
| `SND_RESOURCE_HOLD` | Quieter hold tone for secondary resource POSTs |
| `SND_SERVER_RETRY` | Buzz between failed retry attempts |
| `SND_SAFE_CONFIRM` | Safe cracker exact-match beep |
| `SND_SAFE_PROX` | Safe cracker proximity radar (duration only; freq is dynamic) |

Each entry is `{frequency_hz, duration_ms}`. A frequency of `0` is a silent pause. Volume constants (`_VOL`) are defined next to each array.

---

##  Splash Screen

The device displays a custom 240240 JPEG logo on startup. To change it:
1. Replace `data/Logosmall.jpg` with your image (resize to 240240 JPEG)
2. Run `Documents/ConvertTone.ps1` to regenerate `GuruLogo.h`
3. Recompile and flash

See `Documents/SplashScreen.md` for full instructions.

---

##  Administrative Features

Scan an `ADMIN_BADGE_TAGS` card to enter admin mode. Navigate with the rotary encoder; press to select.

| Option | Function |
|---|---|
| Scan Tag Info | Identify any RFID tag  shows type, location name, resource type |
| Lock/Unlock Prop | HTTP POST to lock/unlock a prop by UUID |
| Toggle Dev Mode | ON: all missions use Guru Home (index 24). OFF: normal random locations |
| Mode Switch | Switch between Mission Widget, Buy Station, Relay |
| Exit | Return to current mode |

> Dev mode cannot bypass scrub cooldowns.

---

##  Mission Scrubbing

Game masters can force-end an active mission:
- Scan a `SCRUB_MISSION_TAGS` card while a mission is in progress
- All current player badges are placed on cooldown (same timer as normal completion)
- `SND_MISSION_SCRUBBED` tone plays to confirm
- Session cleared, device returns to badge-scan screen
- Scrubbed badges display a countdown timer and play `SND_BADGE_COOLDOWN` when scanned
- Scrub cooldowns **cannot** be bypassed by dev mode

---

##  Over-the-Air Updates

- Scan the OTA trigger card to start an update
- Device downloads firmware from GitHub, displays progress (percentage + bytes)
- Installs and reboots automatically

**Deploying new firmware**: Arduino IDE  Sketch  Export Compiled Binary  rename to `mission_M5.ino.bin`  commit to root of `main` branch.

---

##  Security & Anti-Abuse

- Physical RFID badge required for all missions
- Consecutive use prevention (different player must complete between same badge+mission pairs)
- Badge cooldown and scrub system (GM-enforced blocks that dev mode cannot bypass)
- Badge history tracking across power cycles
- Session timeout protection
- Server-side validation of all submissions

---

##  Acknowledgments

Built with M5Stack Dial hardware, ESP32-S3 platform, and the Arduino framework.

---
---

#  Developer Reference

---

## File Structure

| File | Purpose |
|---|---|
| `mission_M5_refactored.ino` | Main state machine, UI, card scan routing, HTTP requests |
| `FreeRoamMission.h/.cpp` | Free roam mission implementation and logic |
| `StoryMission.h/.cpp` | Mission generation, story pages, location/resource tracking |
| `Config.h` | Centralized configuration  tags, locations, endpoints, timeouts |
| `StateManager.h/.cpp` | Persistent storage wrapper (ESP32 Preferences) |
| `Logger.h` | SPIFFS-based timestamped event logging |
| `Sounds.h` | All buzzer tone sequences  edit here only |
| `GuruLogo.h` | PROGMEM JPEG byte array for splash screen |
| `MissionBase.h` | Abstract mission interface |
| `OTAUpdate.h` | Over-the-air firmware update logic |
| `secrets.h` | WiFi credentials (not committed) |

---

## Build & Flash

1. Install **Arduino IDE 2.3.x** with ESP32 board support
2. Select board: **Crabik SLot ESP32-S3**
3. Install required libraries: `M5Dial`, `WiFi`, `HTTPClient`, `Preferences`, `SPIFFS`
4. Set WiFi credentials in `secrets.h` (or directly in `mission_M5.ino`)
5. Configure RFID tags and device identifiers in `Config.h`
6. Upload to device

---

## Configuration (`Config.h`)

| Constant | Default | Description |
|---|---|---|
| `MISSION_TIMEOUT_MIN` | `15` | Mission duration in minutes |
| `HTTP_TIMEOUT` | `5000` | Per-request timeout (ms) |
| `MAX_HTTP_RETRIES` | `3` | Retry attempts per POST |
| `DEVICE_SERIAL` |  | Device serial number sent in payloads |
| `DEVICE_MAC_ADDR` |  | Device MAC sent in payloads |
| `API_ENDPOINT` |  | Mission completion POST URL |
| `REWARD_ENDPOINT` |  | Resource reward POST URL |
| `LOCK_ENDPOINT` |  | Prop lock HTTP URL |
| `UNLOCK_ENDPOINT` |  | Prop unlock HTTP URL |

### RFID Tag Arrays

All tags use space-delimited hex format: `" AA BB CC DD"`

| Array | Purpose |
|---|---|
| `COMPLETE_TAGS[]` | Completion card scan |
| `FULL_RESET_TAGS[]` | Wipe all state + history + log |
| `BADGE_RESET_TAGS[]` | Save state, swap badge, restore |
| `MISSION_CARD_TAGS[]` | Start a HEIST mission |
| `SCRUB_MISSION_TAGS[]` | GM force-ends mission, puts badges on cooldown |
| `ADMIN_BADGE_TAGS[]` | Enter admin mode |
| `POI_LOCATIONS[]` | 24 production locations + index 24 (Guru Home dev) |

Each `POI_LOCATIONS` entry has 4 RFID tags: `weaponTag`, `securityTag`, `vehicleTag`, `moneyTag`.

---

## Location Names

Location names are centralized in `LOCATION_NAMES[]` in `mission_M5.ino` (lines 3156). Edit the string in quotes  the name updates everywhere automatically (story text, admin tag scanner, serial output, mission log).

Index 24 is reserved for `Guru Home` (dev/test location).

---

## Audio System  Developer Notes

### Tone Sequences with Volume Constants

| Array | `_VOL` constant | Trigger |
|---|---|---|
| `SND_BADGE_SCAN` | *(255)* | Badge scanned / any green screen confirm |
| `SND_BADGE_REMOVED` | *(255)* | Player de-registered |
| `SND_MISSION_START` | *(255)* | Mission card accepted |
| `SND_LOCATION_VISIT` | *(255)* | Location tag scanned successfully |
| `SND_MISSION_COMPLETE` | *(255)* | Mission submission fanfare |
| `SND_TIMEOUT` | *(255)* | Mission timer expired |
| `SND_ERROR` | *(255)* | Invalid scan / general error |
| `SND_MISSION_SCRUBBED` | *(255)* | GM scrub card scanned |
| `SND_BADGE_COOLDOWN` | *(255)* | Blocked/scrubbed badge scanned |
| `SND_RESET` | *(255)* | Badge reset / full reset |
| `SND_SENDING_DOTS` | `SND_SENDING_DOTS_VOL` | One blip per dot in "Sending..." animation |
| `SND_SERVER_HOLD` | `SND_SERVER_HOLD_VOL` | Hold tone  primary POST (stopped manually) |
| `SND_RESOURCE_HOLD` | `SND_RESOURCE_HOLD_VOL` | Hold tone  resource POST (quieter, stopped manually) |
| `SND_SERVER_RETRY` | *(255)* | Buzz between failed retry attempts |
| `SND_SAFE_CONFIRM` | `SND_SAFE_CONFIRM_VOL` | Safe cracker exact-match beep |
| `SND_SAFE_PROX` | `SND_SAFE_PROX_VOL` | Proximity radar (duration only; freq overridden at runtime) |

### Hold Tones
`SND_SERVER_HOLD` and `SND_RESOURCE_HOLD` use hardware I2S DMA and keep playing while the CPU blocks on `http.POST()`. Call `startSendHoldTone()` / `startResourceHoldTone()` before the POST and `Speaker.stop()` immediately after. Their `duration` must stay  `HTTP_TIMEOUT`.

### Wrapper Functions (`mission_M5.ino`)
`displayMessage()` calls these internally based on color argument:

| Function | Maps to | Triggered by color |
|---|---|---|
| `playSuccessTone()` | `SND_BADGE_SCAN` | `COLOR_SUCCESS`, `TFT_GREEN` |
| `playErrorTone()` | `SND_ERROR` | `COLOR_ERROR`, `TFT_RED` |
| `playAcceptTone()` | `SND_LOCATION_VISIT` | `COLOR_INFO`, `TFT_BLUE` |
| `playRemoveTone()` | `SND_BADGE_REMOVED` | *(called directly)* |

> Pass `TFT_WHITE` to `displayMessage()` to suppress the automatic tone when you have already played one explicitly.

---

## State Machine

```
WAIT_FOR_BADGE  WAIT_FOR_MISSION_CARD  RUN_MISSION
```

| State | Accepts |
|---|---|
| `WAIT_FOR_BADGE` | Badge tags, admin badge, full/badge reset cards |
| `WAIT_FOR_MISSION_CARD` | Mission cards, badge reset, full reset |
| `RUN_MISSION` | Location tags, completion tag, scrub tag, badge reset, timeout |

---

## Persistence (ESP32 Preferences)

**Namespace `story_mission`**  current session:
- `player_uuid`  Badge UID(s), comma-separated
- `player_class`  Mission category index
- `visited_count`  Locations visited
- `selected_locs`  Selected location indices
- `mission_start_ms`  Timer start time (restored on power loss)
- `locked_difficulty`  Difficulty value locked at timeout
- `saved_state`  Saved tracker state for badge-reset restore
- `saved_ms_start`  Mission start time saved on badge-reset

**Namespace `completed`**  badge cooldown history  
**SPIFFS `/mission_log.txt`**  timestamped event log, auto-truncates at 50KB

---

## Reset Cards

| Card | Effect |
|---|---|
| `FULL_RESET_TAGS` | Wipes all Preferences namespaces + deletes log file |
| `BADGE_RESET_TAGS` | Saves current state, clears badge, prompts re-scan; restores state on new badge |
| `SCRUB_MISSION_TAGS` | Force-ends active mission, places all player badges on scrub cooldown |

---

## Timeout Behavior

- **No locations visited**: Device holds on timeout screen. Resolves only via badge-reset or completion card (both wipe session and clear consecutive-use block).
- **1 location visited**: Difficulty locks at current level. Screen shows "SCAN COMPLETION CARD". Completion card submits the locked difficulty.

---

## Server Communication

### Mission Completion
**POST** `https://iot.rke.world/iot/secondaryMissionComplete`

```json
{
  "mac_address": "10:06:1C:82:FF:44",
  "serial_number": "mutantwarfare2secondary7",
  "difficulty": "Easy|Medium|Hard|Extreme",
  "uuid": "primary_badge_uid",
  "uuids": ["additional_badge_uid"]
}
```

### Resource Reward (one POST per collected resource type)
**POST** `https://iot.rke.world/iot/secondaryMissionComplete`

```json
{
  "mac_address": "10:06:1C:82:FF:44",
  "serial_number": "mutantwarfare2secondary7",
  "difficulty": "WEAPONS|SECURITY|VEHICLE|MONEY",
  "uuid": "primary_badge_uid",
  "uuids": ["additional_badge_uid"]
}
```

Both retry up to `MAX_HTTP_RETRIES` times with `HTTP_TIMEOUT` per attempt. Resource reward failures are non-critical  the mission is still marked complete.

### Audio During Send Sequence
1. Ascending blips (`SND_SENDING_DOTS`)  one per dot in the animation
2. 350Hz hold tone (`SND_SERVER_HOLD`)  plays from start of POST until server responds
3. On retry: short buzz (`SND_SERVER_RETRY`) during the 1-second wait
4. Secondary resource POSTs: 320Hz quieter hold (`SND_RESOURCE_HOLD`)
5. On final 200 response: `SND_MISSION_COMPLETE` fanfare, then pass screen

---

## OTA Updates

Configured in `OTAUpdate.h`:
```cpp
const char* GITHUB_USER = "mawalton910";
const char* GITHUB_REPO = "mission_M5";
const char* FIRMWARE_FILENAME = "mission_M5.ino.bin";
String targetBranch = "main";
```

**Deploy steps**:
1. Arduino IDE  Sketch  Export Compiled Binary
2. Rename to `mission_M5.ino.bin`
3. Commit to root of `main` branch on GitHub
4. Scan OTA trigger card on device  downloads, installs, reboots

---

## Log Viewer

Press BtnA while in `WAIT_FOR_BADGE` state to view `/mission_log.txt` on-device. Auto-closes after 30 seconds or on button press. Full reset clears the log.

---

## Troubleshooting

| Issue | Solution |
|---|---|
| Badge not scanning | Verify space-delimited hex format: `" AA BB CC DD"` |
| Cooldown not expiring | Check `BADGE_COOLDOWN_MS` in `Config.h` |
| Wrong resource scanned | Story highlights required resource in color  scan the matching tag only |
| Timer not persisting after power loss | Verify `stateManager.setMissionStartTime()` is called on mission start |
| WiFi "cannot set config" error | Fixed: `connectToWiFi()` checks existing connection before reconnecting |
| Sound not playing | Verify `M5Dial.Speaker.begin()` in setup; check `_VOL` constants in `Sounds.h` |
| Hold tone keeps playing | Ensure `Speaker.stop()` is called immediately after `http.POST()` returns |
| OTA fails | Verify binary is named `mission_M5.ino.bin` in the root of the `main` branch |
| Log file full | Logger auto-truncates at 50KB; use full reset to clear immediately |
| Dev mode not testing scrub | Scrub cooldowns bypass dev mode by design |

---

**Current Version**: POIStory v26.4.1  
**Last Updated**: April 2026  
