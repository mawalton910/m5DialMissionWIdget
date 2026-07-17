# Copilot Instructions for Mission M5

## Project Overview
ESP32-based RFID mission tracking system for location-based gameplay. Players scan badges for identification, select mission categories via RFID cards, complete randomly-generated missions by visiting real-world locations, then trigger server notifications. Built for M5Dial devices with integrated RFID. Supports two operational modes: Mission Widget and Relay.

## Architecture & Data Flow

### File Structure
```
mission_M5_refactored.ino ← ACTIVE main file — USE THIS
Config.h                ← All constants, tag arrays, types, DisplayText strings, LocationInfo
secrets.h               ← WiFi defaults, API endpoints, device identifiers
StateManager.h/.cpp     ← Persistent storage (ESP32 Preferences)
Logger.h                ← SPIFFS-based event logging
Sounds.h                ← Audio tone definitions (SND_* constants)
OTAUpdate.h             ← Over-the-air firmware update
GuruLogo.h              ← Splash screen logo bitmap
MissionBase.h           ← Abstract mission interface (polymorphic base)
StoryMission.h/.cpp     ← Story/Heist mission type
SafeCrackerMission.h/.cpp ← Safe Cracker mini-game mission
PushMission.h/.cpp      ← Push mission type
ResourceHuntMission.h/.cpp ← Resource Hunt mission type
FreeRoamMission.h/.cpp  ← Free Roam mission type
```

### Core Components (Active)
- **mission_M5_refactored.ino** (main): `StoryTracker` class with 10-state machine driving RFID scanning, 2 operational modes, admin menu, and mission lifecycle
- **Config.h**: ALL constants, RFID tag arrays, `LocationInfo` struct, `POI_LOCATIONS[]`, `DisplayText` namespace, color definitions, enums (`OperationalMode`, `GameDifficulty`, `SafeCrackerDifficulty`, `ResourceType`, `StatusIconType`), `extern` declarations for arrays defined in .ino
- **secrets.h**: WiFi defaults, API endpoint URLs, `DEVICE_SERIAL_NUM`, `DEVICE_MAC_ADDR`, `DEVICE_GAME_ID`
- **MissionBase.h**: Abstract interface (`setup()`, `processLocation()`, `updateDisplay()`, `getCurrentDifficulty()`, `isComplete()`, `findResourceTag()`, `addResource()`)
- **5 mission types**: StoryMission, SafeCrackerMission, PushMission, ResourceHuntMission, FreeRoamMission — all implement `MissionBase`
- **StateManager.h/.cpp**: Persistent storage via ESP32 Preferences (session + completed badge history + saved state for badge-reset)
- **Logger.h**: SPIFFS file `/mission_log.txt`, auto-truncates at 50KB

### Include Dependency Chain
```
mission_M5_refactored.ino
  ├── Config.h → secrets.h
  ├── MissionBase.h
  ├── StoryMission.h
  ├── SafeCrackerMission.h
  ├── PushMission.h
  ├── ResourceHuntMission.h
  ├── FreeRoamMission.h
  ├── StateManager.h
  ├── Logger.h
  ├── OTAUpdate.h
  ├── GuruLogo.h
  └── Sounds.h
```

### Key Data Structures
- **TrackerState enum** (10 states): `WAIT_FOR_BADGE`, `WAIT_FOR_MISSION_CARD`, `RUN_MISSION`, `RELAY_WAIT_BADGE`, `ADMIN_MODE`, `WIFI_CONFIG`, `SELECT_RESOURCE`, `SELECT_DIFFICULTY`, `CONFIRM_PLAYERS`, `SAFE_CRACKER_GAME`
- **OperationalMode enum**: `MODE_MISSION_WIDGET`, `MODE_RELAY`
- **SafeGameSession struct**: 3-tumbler safe cracker game state (combo, dial, hold detection)
- **LocationInfo struct**: name + 4 RFID tags per location (weaponTag, securityTag, vehicleTag, moneyTag)
- **LOCATION_NAMES[]**: Centralized array in .ino (25 total: 24 production + Guru Home dev)
- **RESOURCE_NAMES[]**: Centralized array in .ino
- **DisplayText namespace**: All UI strings in Config.h with inline usage comments

### StoryTracker Class Layout (mission_M5_refactored.ino)
The `StoryTracker` class is the central orchestrator:
- **Lines ~155-260**: Class declaration, public members, constructor, getters/setters, `begin()`, `restoreFromSavedSnapshot()`
- **Lines ~260-680**: `processCardScan()` — handles ALL RFID tag types across all modes
- **Lines ~680-800**: `handleButtonPress()` — button actions per state
- **Lines ~800-950**: `update()` — per-loop-iteration processing (timeouts, encoder, game loops)
- **Lines ~950-1120**: Display functions (admin menu, splash, messages, errors)
- **Lines ~1120-1400**: More display functions (badge prompt, mission card, player registration, log viewer, device info, tag info)
- **Lines ~1400-1600**: Drawing functions (safe cracker V2 renderer, speed adjust, mission passed)
- **Lines ~1600-1700**: Network helpers (WiFi, relay, loot flip, completion POST)
- **Lines ~1700-1800**: State management helpers (resets, badge completion, cooldown, mission category lookup)
- **Lines ~1800-1950**: Private members + helper functions (mission start, timer, safe cracker, admin execute)

### Critical Workflows

#### Mission Completion Flow
1. **Badge scan** → Extract UUID, check cooldown + completion history + consecutive-use block
2. **Mission card scan** → Prevent consecutive badge+mission reuse, determine mission type
3. **Mission run** → Location scanning (RFID tag → location index), resource collection, difficulty calculation
4. **Completion tag** → Validate progress, `sendCombinedCompletionRequest()` (HTTP POST)
5. **On success** → Mark badge completed, reset to badge-wait

#### Mission Timer
- **Duration**: `MISSION_TIMEOUT_MIN` in Config.h (default: 30 minutes)
- **Start**: When mission begins after card scan
- **Display**: BtnA during `RUN_MISSION` shows MM:SS overlay, auto-hides after ~3s
- **Timeout**: No difficulty → blocks until badge-reset or completion card. Difficulty earned → locks difficulty, prompts for completion scan
- **Persistence**: `mission_start_ms` in Preferences, restored on power loss

#### Safe Cracker Mini-Game
- 3-tumbler safe dial game with 3 difficulty levels (Easy/Normal/Extreme)
- Uses `SafeGameSession` struct for state
- Polished V2 renderer with gradient glow bands, vignette rings, dial bezel, hold arc, proximity-colored feedback
- Extreme mode: "BLIND / AUDIO ONLY" (no visual number)
- Speed adjustable via admin menu sub-screen

#### Relay Mode
- Badge scan → send relay update to server
- Switch back to mission via encoder or button

#### Admin Mode (8-item menu)
0. Device Info
1. Mission Widget (mode switch)
2. Relay (mode switch)
3. Dev Mode toggle
4. View Log
5. Scan Tag Info
6. Dial Speed (safe cracker speed adjust)
7. Exit Admin

### Reset Card Hierarchy
- **Full reset**: Wipes session state, completed badge history, log file (`fullReset()`)
- **Badge reset**: Saves tracker state, clears badge UID, prompts new badge + restores previous state
- **Standard reset**: Clears current session, preserves completed history
- **Scrub tag**: Removes current badge from completed/cooldown/scrubbed tracking

### Project-Specific Patterns

#### RFID Tag Matching
- Tags stored as space-delimited hex strings (e.g., `" 6F 3B D5 64"`)
- Match against tag arrays using `NUM_*` constants from Config.h
- Tag categories: `COMPLETE_TAGS`, `FULL_RESET_TAGS`, `BADGE_RESET_TAGS`, `SCRUB_MISSION_TAGS`, `RESET_TAG`, `ADMIN_BADGE_TAGS`, `WIFI_CONFIG_TAGS`, plus mission-type-specific tags

#### Display Text / Newline Handling
- `displayMessage()` parses literal `\\n` (backslash + n in source = two chars `\` + `n`) for line splitting
- In C++ source strings, use `\\n` (escaped backslash) to create the literal two-char sequence
- Real `\n` newlines in strings will be treated as regular characters by the parser (word-wrap only)
- All UI text constants live in `DisplayText` namespace in Config.h

#### State Persistence (Preferences)
- **Session**: `story_mission` namespace — player UUID, locations, visit count, mission start time, locked difficulty
- **Badge history**: `completed` namespace
- **Saved state**: For badge-reset restore — tracker state, mission start time, locked difficulty
- Full reset clears all namespaces + deletes `/mission_log.txt`

### Server Communication
- **Mission completion**: POST to `MISSION_API_ENDPOINT` (secrets.h) with badge UUID, category, difficulty, respect
- **Relay**: POST to `RELAY_UPDATE_URL`
- **Retry**: Up to 3 attempts, 5-second timeout per request
- **WiFi**: Auto-reconnect, 10s timeout, supports stored credentials via StateManager

### M5Dial Hardware
- RFID: Built-in MFRC522 module
- Display: 240×240 LCD via M5 library
- Encoder: Rotary dial for UI navigation
- Speaker: Buzzer for audio feedback tones
- BtnA: Button for context-sensitive actions

## Development Helpers

### Compilation
- Board: "Crabik SLot ESP32-S3"
- Includes: M5Dial, WiFi, HTTPClient, Preferences, SPIFFS, ArduinoJson, standard C++ (vector, algorithm, map, set, random)

### Adding a New Mission Type
1. Create `YourMission.h` and `YourMission.cpp` implementing `MissionBase`
2. Add RFID tag array in `Config.h` (e.g., `YOUR_MISSION_TAGS[]` + `NUM_YOUR_MISSION_TAGS`)
3. In `mission_M5_refactored.ino`:
   - Add `#include "YourMission.h"`
   - Add tag matching in `processCardScan()` under the `WAIT_FOR_MISSION_CARD` section
   - Call `startMission(new YourMission(stateManager), ...)` with any setup lambda
4. See existing missions (PushMission, FreeRoamMission) for simple examples

### Adding New Locations
- Add name to `LOCATION_NAMES[]` array in .ino file — maintains index order
- Add corresponding `LocationInfo` entry to `POI_LOCATIONS[]` in Config.h with matching index
- Define exactly 4 unique RFID tags per location
- Index 24 reserved for Guru Home (dev location)

### Editing Location Names
- All 25 names centralized in `LOCATION_NAMES[]` array in .ino
- Edit the string in quotes — name updates throughout entire codebase
- No need to modify Config.h, StoryMission.cpp, or any other files

### Configuration via Config.h
- `MISSION_TIMEOUT_MIN`: Mission duration in minutes (default: 30)
- `BADGE_COOLDOWN_MS`: Cooldown between badge scans
- Display colors: `COLOR_ERROR`, `COLOR_SUCCESS`, `COLOR_WARNING`, `COLOR_INFO`, `COLOR_PROCESSING`
- All RFID tag arrays and `NUM_*` constants
- `DisplayText` namespace for all UI strings

### Testing & Debugging
- **Log Viewer**: BtnA in badge-wait state → view persistent log with timestamps
- **Serial output**: State transitions, tag matches, location progress, timer events
- **Dev Mode**: Toggle via admin menu — bypasses certain checks
- **Full reset tag**: Clears all state + completed badges + log in one operation
- **Badge reset tag**: Test state restoration after badge swap or power loss

---

**Last Updated**: January 2025 | **Version**: v5 Cleaned
