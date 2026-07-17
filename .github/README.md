# POI Story Dual Send No Sound v4

ESP32-based RFID mission tracking system for location-based gameplay. Players scan badges for identification, select mission categories via RFID cards, complete randomly-generated story missions by visiting real-world locations, then trigger server notifications. Built for M5Dial devices with integrated RFID.

## Quick Start

1. Open `POIStoryDualSendNoSoundv4.ino` in Arduino IDE
2. Install required libraries: M5Dial, WiFi, HTTPClient, Preferences, SPIFFS
3. Configure WiFi in sketch (WIFI_SSID, WIFI_PASSWORD defined in .ino)
4. Configure your RFID tags in `Config.h` (see RESOURCE_TAG_TEMPLATE.md)
5. Upload to M5Dial (board: "Crabik SLot ESP32-S3")
6. For testing: Use admin mode to enable dev mode (forces GURU_HOME location)

## Configuration

Edit `Config.h` to customize:
- **MISSION_TIMEOUT_MIN**: Mission duration (default: 45 minutes)
- **DEVICE_SERIAL**, **DEVICE_MAC**: Device identifiers
- **WIFI_SSID**, **WIFI_PASSWORD**: Network credentials (defined in .ino) - used as defaults
- **API_ENDPOINT**, **REWARD_ENDPOINT**, **LOCK_ENDPOINT**, **UNLOCK_ENDPOINT**: Server URLs
- **RFID Tags**: Badge, mission, location (4 resource tags each), and control card definitions
- **POI_LOCATIONS**: 24 production locations + GURU_HOME (dev location at index 24)

### WiFi Configuration via NFC Tags

The device supports dynamic WiFi configuration using NFC tags, allowing easy network switching without code changes:

**Setup Process**:
1. **Trigger WiFi Config Mode**: Scan a WiFi config tag (`WIFI_CONFIG_TAGS` in Config.h)
2. **Scan SSID Tag**: Scan an NFC tag whose UID will be used as the WiFi SSID
3. **Scan Password Tag**: Scan an NFC tag whose UID will be used as the WiFi password
4. **Saved**: WiFi credentials persist in ESP32 flash (Preferences)

**Behavior**:
- **Stored credentials**: Device attempts stored WiFi first
- **Fallback**: If no stored credentials or connection fails, uses default WIFI_SSID/WIFI_PASSWORD from code
- **Persistence**: WiFi credentials survive power loss and persist through full reset
- **Exit config mode**: Scan WiFi config tag again to exit without saving
- **Clear stored WiFi**: Scan WiFi config tag to reconfigure, or manually clear via `stateManager.clearWiFiCredentials()`

**Status Logging**:
- Connection attempts logged to `/mission_log.txt`
- Serial monitor shows which credentials being used (stored vs default)

## Mission Flow

1. **Scan badge** → Player identification
2. **Scan mission card** → Category selection (Heist, Police, Hacking, Job, Vehicle, Drug, Medical, Community)
3. **Read story** → Mission generates 4 random locations with resource-specific objectives
4. **Visit locations** → Scan the specific resource tag (WEAPONS, SECURITY, VEHICLE, or MONEY) at each location
5. **Scan completion card** → Submit mission and earned difficulty to server
6. **Server approval** → Badge marked completed, return to badge scan

### Resource System
Each of the 24 production locations has **4 RFID tags** (one per resource type):
- **WEAPONS** (red highlight in story)
- **SECURITY** (yellow highlight)
- **VEHICLE** (cyan highlight)
- **MONEY** (green highlight)

Missions randomly select 4 locations and assign one resource type to each. Players must scan the **correct resource tag** at each location. Story text dynamically includes the resource name (e.g., "Secure the **WEAPONS** at Renaissance Hub").

## Mission Timer

- **Default**: Configurable in `Config.h` (currently set to 45 minutes)
- **Display**: On-demand MM:SS overlay (BtnA during mission). Renders via off-screen sprite for flicker-free draw, auto-hides after ~3 seconds, and redraws the mission view when hiding. Shows "HURRY!" in red when < 60 seconds remain.
- **Timeout Behavior**:
  - No difficulty earned → Device holds a timeout screen and blocks other scans until you scan either a badge-reset card or a completion card. That scan clears the session and also clears the consecutive-use block for the just-timed-out pair.
  - Difficulty earned → Difficulty locks, display shows "SCAN COMPLETION CARD", completion card submits locked difficulty
- **Persistence**: Saved across power loss and badge-reset operations

## Persistence & State Recovery

### Preferences (Persisted in ESP32 Flash)

**Namespace `story_mission`** (current session):
- `player_uuid` — Current badge UID
- `player_class` — Selected mission category index
- `visited_count` — Number of locations visited
- `selected_locs` — Selected location indices (string)
- `mission_start_ms` — Mission start time (milliseconds) for timer restoration
- `locked_difficulty` — Difficulty value when mission times out with progress
- `saved_state` — Saved tracker state for badge-reset restore
- `saved_ms_start` — Mission start time for badge-reset timer restore

**Namespace `completed`** (badge history):
- Badge UIDs as keys, boolean `true` values indicating completion
- Persists across device resets and full resets (if preserved)

### SPIFFS File System (Persisted on Device)

- **`/mission_log.txt`** — Timestamped event log
  - Logs all card scans, state transitions, completions, errors
  - Auto-truncates at ~50KB to preserve space
  - Viewed via button press in badge-wait state
  - Cleared on full reset

### Example State Recovery Scenarios

**Scenario 1: Power Loss During Mission**
1. Device loses power mid-mission
2. On restart, `mission_start_ms` restored from Preferences
3. Timer resumes from persisted start time
4. Mission continues; timer counts down from remaining time

**Scenario 2: Badge Swap During Mission**
1. Scan badge-reset card while mission active
2. Current state saved: `saved_state` = RUN_MISSION, `saved_ms_start` = mission_start_ms, `locked_difficulty` persisted
3. Scan new badge
4. Previous state restored: mission display resumes, timer resumes counting down
5. Locked difficulty (if any) restored and ready for completion scan

## Reset & Control Cards

**Full Reset** (`FULL_RESET_TAGS`):
- Wipes all session state and completed badge history
- Clears mission log file
- Returns to badge-scan screen immediately

**Badge Reset** (`BADGE_RESET_TAGS`):
- Saves current tracker state and mission timer
- Clears current badge UID
- Prompts for new badge scan
- On new badge: restores previous state (mission selection or active mission)
- After a no-progress timeout: scanning a badge-reset card clears the timeout hold screen, wipes the session, and removes the consecutive-use block for the just-timed-out badge+mission pair

**Standard Reset** (`RESET_TAG`):
- Clears current session (badge + mission)
- Preserves completed badge history
- Returns to badge-scan screen

**Completion Card after no-progress timeout**:
- If the mission timed out with zero locations visited, scanning a completion card (instead of a badge-reset card) also clears the timeout hold screen, wipes the session, and removes the consecutive-use block for that badge+mission pair.

## Features

### Admin Mode
Scan an admin badge (configured in `ADMIN_BADGE_TAGS`) to enter admin mode with 3 menu options:

1. **Scan Tag Info**: Identify any RFID tag
   - Scan admin badge → turn dial to option 1 → press button → scan any tag
   - Displays tag type (mission card, location resource, control card, etc.)
   - Shows location name and resource type for location tags
   - Large text on black background for easy reading

2. **Lock/Unlock Prop**: HTTP control for props
   - Navigate to option 2 → press button → choose lock or unlock → scan prop badge
   - Sends HTTP POST to `LOCK_ENDPOINT` or `UNLOCK_ENDPOINT` with UUID
   - Useful for field testing and prop management

3. **Toggle Dev Mode**: Enable/disable development testing mode
   - Navigate to option 3 → press button to toggle
   - **ON** (green): All missions use GURU_HOME location only (index 24)
   - **OFF** (red): Normal random location selection from 24 production locations
   - Dev mode allows testing all 4 resource scans at a single location without travel
   - Status persists until toggled or device resets

**Encoder Navigation**: Turn dial to select option (threshold: 4 ticks), press button to confirm
**Exit Admin Mode**: Scan admin badge again at any time

### Log Viewer
- Press BtnA while in badge-wait state
- Displays mission log with timestamps
- Auto-closes after 30 seconds or on button press

### Server Communication
- HTTP POST to `API_ENDPOINT` for mission completion
- HTTP POST to `REWARD_ENDPOINT` for reward distribution
- 3 retry attempts with 5-second timeout
- Both must return HTTP 200 for success

### Display & Feedback
- Color-coded screens: RED=error, GREEN=success, ORANGE=warning, BLUE=info, DARK_BLUE=normal
- M5Dial encoder for page navigation during story missions
- Wrapped text rendering for multi-line prompts
- Splash screen stays visible while storage/logger initialize; "Tap Badge" appears only after logger is ready
- UI text is centralized in the `DisplayText` namespace inside `Config.h` with a usage reference comment to locate each string's caller.

## Development & Customization

### File Structure
- **POIStoryDualSendNoSoundv4.ino** — Main state machine and UI
- **StoryMission.h/.cpp** — Mission generation and story pages
- **Config.h** — Centralized configuration (tags, locations, endpoints)
- **StateManager.h** — Persistent storage wrapper (Preferences)
- **Logger.h** — SPIFFS-based event logging
- **MissionBase.h** — Mission interface
- **.github/copilot-instructions.md** — AI agent guidance

### Changing Location Names (EASY!)
Location names are centralized in `POIStoryDualSendNoSoundv4.ino` for easy editing:

**Edit lines 31-56** in the .ino file:
```cpp
const char* LOCATION_NAMES[] = {
    "Belle Isle Outpost",     // 0 - Change this!
    "Brush Park",             // 1 - Change this!
    // ... edit any name ...
    "Guru Home"               // 24 - DEV LOCATION
};
```

Just change the text in quotes - names update everywhere automatically (story text, admin mode, serial output, logs).

### Adding New Locations
1. Add name to `LOCATION_NAMES[]` array in .ino file (maintain index order)
2. Add entry to `POI_LOCATIONS` array in Config.h with matching index
3. Define exactly 4 RFID tags per location (weaponTag, securityTag, vehicleTag, moneyTag)
4. Follow space-delimited hex format: `" AA BB CC DD"`
5. GURU_HOME (index 24) is reserved for development testing

**Location Structure in Config.h**:
```cpp
{ LOCATION_NAMES[index],     // References name from .ino
  " AA BB CC DD",           // WEAPONS tag
  " EE FF 00 11",           // SECURITY tag  
  " 22 33 44 55",           // VEHICLE tag
  " 66 77 88 99" }          // MONEY tag
```

### Changing Mission Timeout
Edit Config.h:
```cpp
const int MISSION_TIMEOUT_MIN = 45;  // Change to desired minutes (currently 45)
```

### Development Mode (GURU_HOME)
For testing without traveling to production locations:
1. Enter admin mode (scan admin badge)
2. Navigate to option 3 (Toggle Dev Mode)
3. Press button to enable
4. Exit admin mode and start new mission
5. All 4 objectives will target GURU_HOME location
6. Scan all 4 resource tags at GURU_HOME to complete mission
7. Toggle off when ready for production testing

## API Integration

### Mission Completion Endpoint
**POST** `https://iot.rke.world/iot/secondaryMissionComplete`

Payload:
```json
{
  "mac_address": "10:06:1C:82:FF:44",
  "serial_number": "mutantwarfare2secondary7",
  "difficulty": "Easy|Medium|Hard|Extreme",
  "uuid": "badge_uid_hex",
  "game": "681ae10fac40fb2222f9c27c"
}
```

### Reward Endpoint
**POST** `http://iot.rke.world/iot/itemIncrementer`

Payload:
```json
{
  "mac_address": "10:06:1C:82:FF:44",
  "serial_number": "mutantwarfare2secondary7",
  "uuid": "badge_uid_hex",
  "item_id": "category_item_id",
  "amount": 1,
  "game": "681ae10fac40fb2222f9c27c"
}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Mission timer not persisting | Verify `StateManager.setMissionStartTime()` called on mission start |
| Locked difficulty not submitting | Check `lockedDifficulty` populated; ensure completion card scanned |
| Log file too large | Logger auto-truncates at 50KB; check `MAX_LOG_SIZE` in Logger.h |
| WiFi timeout | Verify SSID/password; increase `WIFI_CONNECT_TIMEOUT` in Config.h |
| WiFi "cannot set config" error | Fixed: connectToWiFi() now checks existing connection before reconnecting |
| WiFi connection verification | Check serial monitor for "WiFi Connected! IP: x.x.x.x" or view mission log |
| Badge won't scan | Ensure tag format matches space-delimited hex (e.g., " 6F 3B D5 64") |
| Wrong resource scanned | Story shows required resource in color; must scan matching tag at location |
| Dev mode not working | Verify admin mode toggle shows "DEV MODE: ON" (green); GURU_HOME must have tags configured |

---

**Version**: v4 (No Sound variant)  
**Last Updated**: December 2025  
**Platform**: ESP32 (Crabik SLot ESP32-S3) with M5Dial
```cpp
// Change these to match your device
const String DEVICE_SERIAL = "mutantwarfare2secondary7";
const String DEVICE_MAC = "10:06:1C:82:FF:44";
```

#### WiFi Settings (`POIStoryDualSendNoSoundv4.ino`)
```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

#### Mission Categories
Update category names in both files:
```cpp
// In POIStoryDualSendNoSoundv4.ino
const char* MISSION_CATEGORY_NAMES[] = {
    "Heist", "Police", "Hacking", "Job", 
    "Vehicle", "Drug", "Medical", "Community"
};

// In POIStoryDualSendNoSoundv4.ino  
const char* CATEGORY_ITEM_IDS[] = {
    "your_heist_item_id",
    "your_police_item_id", 
    // ... etc
};
```

#### Locations (`Config.h`)
Replace the 24 POI locations with your real-world locations:
```cpp
const LocationInfo POI_LOCATIONS[] = {
    { "Your Location Name",
      { " AA BB CC DD", " EE FF 00 11", " 22 33 44 55", " 66 77 88 99" },
      4 },
    // Add your 24 locations here
};
```

#### RFID Tag Configuration (`Config.h`)
Replace placeholder tags with your actual RFID tag values:

**Mission Completion Tags**:
```cpp
const String COMPLETE_TAGS[] = {
    " EF 96 E2 64", " 6F 14 E4 64", // Add your completion tags
};
```

**Mission Category Tags**:
```cpp
const String MISSION_CARD_TAGS[8][10] = {
    // HEIST category tags
    { " 86 10 EA AD", " 01 02 03 05", /* add 8 more */ },
    // Add tags for all 8 categories
};
```

**Reset Card Tags**:
```cpp
// Badge reset cards (allow re-scanning badge)
const String BADGE_RESET_TAGS[] = {
    " A1 B2 C3 D4", " E5 F6 07 08"  // Replace with your tags
};

// Mission reset cards (removed in v4; use badge reset + re-scan or standard reset instead)
```

#### Reward System
Modify reward amounts based on difficulty:
```cpp
// In sendCombinedCompletionRequest()
if (difficulty == "Easy") amount = 2;
else if (difficulty == "Medium") amount = 4;
else if (difficulty == "Hard") amount = 6;
else if (difficulty == "Extreme") amount = 10;
```

#### Story Content (`StoryMission.h`)
Customize the dynamic story phrases for each mission category:
```cpp
const char* HEIST_ACTIONS[4] = {
    "Breach the vault at",
    "Disable the alarm at", 
    // Customize these phrases
};
```

### ⚠️ Critical Server Communication Settings

**DO NOT MODIFY** these server communication elements unless you update your backend:

#### API Endpoints
```cpp
const String API_ENDPOINT = "https://iot.rke.world/iot/secondaryMissionComplete";
// Second endpoint: "http://iot.rke.world/iot/itemIncrementer"
```

#### JSON Payload Structure
The server expects these exact field names:
```json
// Mission completion payload
{
   "mac_address": "device_mac",
   "serial_number": "device_serial", 
   "difficulty": "Easy|Medium|Hard|Extreme",
   "uuid": "player_badge_id",
   "game": "game_id"
}

// Reward payload  
{
   "mac_address": "device_mac",
   "serial_number": "device_serial",
   "uuid": "player_badge_id", 
   "item_id": "category_item_id",
   "amount": numeric_value,
   "game": "game_id"
}
```

#### Game ID
```cpp
const String GAME_ID = "681ae10fac40fb2222f9c27c";  // Must match your server
```

## File Structure

```
POIStoryDualSendNoSoundv4/
├── POIStoryDualSendNoSoundv4.ino  # Main Arduino sketch
├── Config.h                        # Configuration and RFID tags
├── MissionBase.h                   # Abstract mission interface
├── StoryMission.h                  # Story mission class definition
├── StoryMission.cpp                # Story mission implementation  
├── StateManager.h                  # Persistent state management
├── POIMission.h                    # Alternative POI mission (unused)
└── README.md                       # This file
```

## Key Classes

- **StoryTracker**: Main controller handling RFID scanning and state management
- **StoryMission**: Generates dynamic stories and tracks location progress
- **StateManager**: Handles persistent storage of mission state and completed badges
- **MissionBase**: Abstract interface for different mission types

## Installation

1. Install Arduino IDE with ESP32 board support
2. Install required libraries: M5Dial, WiFi, HTTPClient, Preferences
3. Select "Crabik SLot ESP32-S3" as board type
4. Update all configuration variables with your values
5. Replace all placeholder RFID tags with your actual tag values
6. Upload to your M5Dial device

## Usage

1. Power on device - shows "Tap Badge" screen
2. Scan player badge for identification  
3. Scan mission category card to select mission type
4. Read generated story and visit the 4 specified locations
5. Scan RFID tags at each physical location
6. Return and scan completion tag when done
7. System sends data to server and plays success sound

## Advanced Features

### State Recovery
If the device loses power during a mission, it automatically restores the player's progress when restarted.

### Consecutive Use Prevention  
Only the most recently completed/failed badge+mission pair is blocked in-memory. A badge becomes reusable after any other badge runs a mission (or times out). After a no-progress timeout, the pair is blocked until you scan a badge-reset or completion card, both of which clear that block.

### Reset Functionality
- **Badge Reset Cards**: Allow re-scanning badge if wrong one was used
- **Mission Reset Cards**: Allow re-scanning mission card if wrong category selected
- **Master Reset**: Clears current session
- **Factory Reset**: Wipes all data and restarts device

## Troubleshooting

### Common Issues
- **"Missing required fields" error**: Check that all server payload fields match exactly
- **WiFi connection failures**: Verify SSID/password and signal strength  
- **RFID not reading**: Ensure tags are close to device and properly formatted
- **Mission not progressing**: Check that location RFID tags match Config.h values

### Debug Output
Enable Serial Monitor (115200 baud) to view detailed operation logs including:
- RFID tag values as they're scanned
- Server communication attempts and responses
- Mission state changes and location visits

## Contributing

When contributing:
1. Test all RFID scanning functionality
2. Verify server communication with both endpoints
3. Ensure state persistence works correctly across power cycles
4. Test consecutive use prevention logic
5. Validate all reset card functionality

## License

[Add your preferred license here]

## Server Integration

This device requires a compatible backend server that accepts the specified JSON payloads at the configured endpoints. The server should handle:
- Mission completion logging
- Player reward distribution  
- Game state management
- Player progress tracking

Contact the project maintainer for server setup documentation.
