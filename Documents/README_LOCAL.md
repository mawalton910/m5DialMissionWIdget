# Mission M5 - POI Story System
Current Version: 4.0 (No Sound variant)

ESP32-based RFID mission tracking system for location-based gameplay. Players scan badges for identification, select mission categories via RFID cards, complete randomly-generated story missions by visiting real-world locations, then trigger server notifications. Built for M5Dial devices with integrated RFID.

## 🎮 Overview

The Mission M5 is a specialized RFID-enabled device that allows players to:
- Scan badges for player identification
- Select mission categories (Heist, Police, Hacking, Job, Vehicle, Drug, Medical, Community)
- Complete randomly-generated story missions by visiting real-world locations
- Track mission progress with timer and location visits
- Submit completed missions for respect rewards
- View mission logs and device information

## 🔧 Hardware Requirements

- **M5Stack Dial** (ESP32-S3)
- Built-in **PN532 NFC/RFID Reader**
- 240x240 Round Display
- Rotary Encoder with Button
- WiFi Connectivity (2.4GHz)

## 🚀 Quick Start
📦 Features

### Mission System
## ⚙️ Configuration
MISSION_TIMEOUT_MIN**: Mission duration (default: 30 minutes)
- **DEVICE_SERIAL**, **DEVICE_MAC**: Device identifiers
- **API_ENDPOINT**, **REWARD_ENDPOINT**, **LOCK_ENDPOINT**, **UNLOCK_ENDPOINT**: Server URLs
- **RFID Tags**: Badge, mission, location (4 resource tags each), and control card definitions
- **POI_LOCATIONS**: 24 production locations + GURU_HOME (dev location at index 24)

### WiFi Settings

**In mission_M5.ino**:
```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

Edit `Config.h` to customize:tegory-specific phrases
- **Location Tracking**: Visit 4 random locations per mission (25 total locations available)
- **Mission Timer**: 30-minute countdown with on-demand display and timeout handling
- **Difficulty System**: Earn difficulty levels based on locations visited
- **Respect Rewards**: Dynamic rewards calculated based on mission difficulty
- **Badge Reset**: Save and restore mission state when switching players

### Mission Categories
- **Heist**: High-stakes robbery operations
- **Police**: Law enforcement missions
- **Hacking**: Digital infiltration tasks
- **Job**: Professional assignments
- **Vehicle**: Transportation-based missions
- **Drug**: Pharmaceutical operations
- **Medical**: Healthcare-related tasks
- **Community**: Social service missions

### Resource System
Each of the 24 production locations has **4 RFID tags** (one per resource type):
- **WEAPONS** (red highlight in story)
- **SECURITY** (yellow highlight)
- **VEHICLE** (cyan highlight)
- **MONEY** (green highlight)

Missions randomly select 4 locations and assign one resource type to each. Players must scan the **correct resource tag** at each location. Story text dynamically includes the resource name (e.g., "Secure the **WEAPONS** at Renaissance Hub").
iFi Lookup Table Guide for detailed preset usage

## 🎯 📡 OTA Updates

### Triggering an Update

1. Scan designated OTA trigger RFID card
2. Device connects to OTA WiFi (can be different from normal WiFi)
3. Downloads firmware from GitHub: `https://raw.githubusercontent.com/[USER]/mission_M5/main/mission_M5.ino.bin`
4. Displays progress with percentage and download size
5. Installs and reboots automatically

### Deploying New Firmware

1. **Export Compiled Binary**:
   - Arduino IDE → Sketch → Export Compiled Binary
   
2. **Rename the file** to: `mission_M5.ino.bin`

3. **Upload to GitHub**:
   - Place file in the **root** of the `main` branch (or your configured branch)
   - Commit and push changes

4. **Deploy**:
   - Scan the OTA trigger card on any device
   - Track progress** → View timer with BtnA during mission
6. **Scan completion card** → Submit mission and earned difficulty to server
7
### OTA Configuration

Configure in `OTAUpdate.h`:
```cpp
// OTA Trigger Card (scan to start update)
const String OTA_TRIGGER_UIDS[] = {
    "04B6C5D65F6180",  // Add more trigger cards as needed
};

// ⏱️ Mission Timer

- **Default**: Configurable in `Config.h` (default: 30 minutes)
- **Display**: On-demand MM:SS overlay (BtnA during mission). Renders via off-screen sprite for flicker-free draw, auto-hides after ~3 seconds, redraws
// GitHub Settings
const char* GITHUB_USER = "mawalton910";
const char* GITHUB_REPO = "mission_M5";
const char* FIRMWARE_FILENAME = "mission_M5.ino.bin";
String targetBranch = "main";
```💾 
### Administrative Features
- **Device Info**: Serial number, MAC address, firmware version
- **WiFi Management**: Network scanning, connection, and credential storage via NFC tags
- **Loot Flip Mode**: Lock/unlock loot items by UUID
- **Tag Scanner**: Identify any RFID card's purpose and location
- **Mission Log Viewer**: Review all mission events with timestamps
- **Buy Station Mode**: Integrated loot claiming system
- **Development Mode**: Test with Guru Home location (index 24)

### Over-the-Air (OTA) Updates
- **Remote firmware updates** via GitHub
- Triggered by scanning designated RFID tag
- Automatic download and installation from GitHub repository
- Progress display with percentage and download stats
- Automatic reboot after successful update
- Can use different WiFi network for updates via Arduino Library Manager:
   - M5Dial
   - WiFi (ESP32 core)
   - HTTPClient
   - ArduinoJson
   - WiFiClientSecure
   - HTTPUpdate
   - Preferences
   - SPIFFS
3. Configure WiFi in sketch (WIFI_SSID, WIFI_PASSWORD defined in .ino)
4. Configure your RFID tags in `Config.h` (see sections below)
5. Select board: **"Crabik SLot ESP32-S3"**
6. Upload to M5Dial device
7. For testing: Use admin mode to enable dev mode (forces GURU_HOME location)

## Configuration

Edit `Config.h` to customize:
- **MISSION_TIMEOUT_MIN**: Mission duration (default: 45 minutes)
- **DEVICE_SERIAL**, **DEVICE_MAC**: Device identifiers
- **WIFI_SSID**, **WIFI_PASSWORD**: Network credentials (defined in .ino) - used as defaults
- **API_ENDPOINT**, **REWARD_ENDPOINT**, **LOCK_ENDPOINT**, **UNLOCK_ENDPOINT**: Server URLs
- **RFID Tags**: Badge, mission, location (4 resource tags each), and control card definitions
- **POI_LOCATIONS**: 24 production locations + GURU_HOME (dev location at index 24)

###🔄  WiFi Configuration via NFC Tags (Lookup Table)

The device supports dynamic WiFi configuration using a **preset lookup table**, allowing easy network switching with a single tag scan:

**Setup (One-Time)**:
1. **Edit Config.h**: Add your WiFi networks to the `WIFI_PRESETS[]` array:
   ```cpp
   const WiFiCredential WIFI_PRESETS[] = {
       {" 12 34 56 78", "HomeWiFi", "HomePass123"},    // Home network
       {" AB CD EF 01", "OfficeGuest", "OfficePass"},  // Work network
       {" 11 22 33 44", "EventWiFi", "EventPass789"},  // Event venue
   };
   ```
2. **Get Tag UIDs**: Use admin mode "Scan Tag" to identify your NFC tags
3. **Upload Code**: Flash updated Config.h to device

**Field Use (Simple)**:
1. **Scan WiFi config trigger tag** → Enter WiFi config mode
2. **Scan preset WiFi tag** → Device looks up SSID/password and saves them
3. **Done!** Device connects using saved credentials

**Behavior**:
- *🛠️ Admin Mode

Scan an admin badge (configured in `ADMIN_BADGE_TAGS`) to enter admin mode. Navigate using rotary encoder
- **Exit config mode**: Scan WiFi config trigger tag again to exit without saving
- **Reconfigure**: Scan trigger + different preset tag to change networks
- **Rotate**: Scroll through menu
- **Press**: Select option
- **Scan Admin Badge Again**: Exit admin mode

Available admin options:
1. **Device Info**: View serial number, MAC address, firmware version, current mode, WiFi status and IP
2. **Mission Widget Mode**: Switch to story mission mode
3. **Buy Station Mode**: Switch to loot claiming system
4. **Relay Mode**: Switch to badge relay mode
5. **Exit Admin**: Return to current mode's initial state

### Mission*MONEY** (green highlight)

Missions randomly select 4 locations and assign one resource type to each. Players must scan the **correct resource tag** at each location. Story text dynamically includes the resource name (e.g., "Secure the **WEAPONS** at Renaissance Hub").

## Mission Timer

- **Default**: Configurable in `Config.h` (currently set to 45 minutes)
- **Display**: On-demand MM:SS overlay (BtnA during mission). Renders via off-screen sprite for flicker-free draw, auto-hides after ~3 seconds, and redraws the mission view when hiding. Shows "HURRY!" in red when < 60 seconds remain.
- **Timeout Behavior**:
   🌐 API Integration

The system integrates with a backend API for: → Device holds a timeout screen and blocks other scans until you scan either a badge-reset card or a completion card. That scan clears the session and also clears the consecutive-use block for the just-timed-out pair.
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
c RFID card UIDs
- API request/response logs
- OTA update progress
- Error messages and stack traces
- Mission state changes and location visits

## ⚙️ Advanced Configuration

## API Integration

### Mission Completion Endpoint
**POST** `https://iot.rke.world/iot/secondaryMissionComplete`
### Device Identifiers (Config.h)
```cpp
const char* DEVICE_SERIAL = "mutantwarfare2secondary7";
const char* DEVICE_MAC = "10:06:1C:82:FF:44";
```

### WiFi Settings (misand IDs:
```cpp
// In mission_M5tworkName";
const char* WIFI_PASSWORD = "YourPassword";
```

### Mission Categories (mission_M5.ino)antwarfare2secondary7",
  "difficulty": "Easy|Medium|Hard|Extreme",
  "utem IDs for reward system
const char* CATEGORY_ITEM_IDS[] = {
    "686c6743646b5c566738aa89",  // HEIST
```686c672e646b5c566738a9de",  // POLICE
    // ... etc
};
```

### Locations (Config.h
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
|-- RFID Tag Configuration (Config.h
| Mission timer not persisting | Verify `StateManager.set (space-delimited hex format)MissionStartTime()` called on mission start |
| Locked difficulty not submitting | Check `lockedDifficulty` populated; ensure completion card scanned |
| Log file too large | Logger auto-truncates at 50KB; check `MAX_LOG_SIZE` in Logger.h |
| WiFi timeout | Verify SSID/password; increase `WIFI_CONNECT_TIMEOUT` in Config.h |
| WiFi "cannot set config" error | Fixed: connectToWiFi() now checks existing connection before reconnecting |
| WiFi connection verification | Che // 8 completion tags total Connected! IP: x.x.x.x" or view mission log |
| Badge won't scan | Ensure tag format matches space-delimited hex (e.g., " 6F 3B D5 64") |
| Wrong resource scanned | Story shows required resource in color; must scan matching tag at location |
| Dev mode not working | Verify admin mode toggle shows "DEV MODE: ON" (green); GURU_HOME must have tags configured |

---

**Version**: v4 (No Sound  (10 tags per category)
    { " 86 10 EA AD", /* ... 9 more tags */ },
    // ... 7 more categories
};
```

**Reset Control CardICE_MAC = "10:06:1C:82:FF:44";
```

const String BADGE_RESET_TAGS[] = {
    " A1 B2 C3 D4", " E5 F6 07 08"
};

// Full reset cards  
const String FULL_RESET_TAGS[] = {
    " 11 22 33 44"
};
```

### Reward System (mission_M5.ino)mes function
if (difficulty == "Easy") amount = 2;
else if (difficulty == "Medium") amount = 4;
else if (difficulty == "Hard") amount = 6;
else if (difficulty == "Extreme") amount = 10;
```

### Story Content (StoryMission.h
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
Re 🔒 Critical Server Communication Settings

**⚠️ DO NOT MODIFY** these server communication elements unless you update your backend:

### API Endpoints (Config.h)ETE_TAGS[] = {
    " EF 96 E2 64", " 6F 14 E4 64", // Add your completion tags
};
const String REWARD_ENDPOINT = "http://iot.rke.world/iot/itemIncrementer";
```

``cpp
const String MISSION_CARD_TAGS[8][10] = {
    // HEIST category tags
    { " 86 10 EA AD",
{
   "mac_address": "device_mac",
   "serial_number": "device_serial", 
   "difficulty": "Easy|Medium|Hard|Extreme",
   "uuid": "player_badge_id",
   "game": "game_id"
}

// Reward distribution
{
   "mac_address": "device_mac",
   "serial_number": "device_serial",
   "uuid": "player_badge_id", 
   "item_id": "category_item_id",
   "amount": numeric_value,
   "game": "game_id"
}
```

### Game ID (Config.h)ficulty == "Medium") amount = 4;
else if (difficulty == "Hard") amount = 6;
else if (difficulty == "Extreme") amount = 10;
```

###📚 Additional Features

### State Recovery
If the device loses power during a mission, it automatically restores the player's progress when restarted.

### Consecutive Use Prevention  
Only the most recently completed/failed badge+mission pair is blocked in-memory. A badge becomes reusable after any other badge runs a mission (or times out). After a no-progress timeout, the pair is blocked until you scan a badge-reset or completion card, both of which clear that block.

## 📄 License

[Add your preferred license here]

## 👥 Contributors

- [Your Name/Team]

## 🤝
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
6. Test OTA update process

## 📧 Support

For issues and questions:
- Open an issue on GitHub
- [Add contact information]

## 🙏 Acknowledgments

- Built with M5Stack Dial hardware
- ESP32-S3 platform (Crabik SLot ESP32-S3)
- Arduino framework
- M5Dial library by M5Stack

---

**Current Version**: Mission M5 v4.0 (POI Story Dual Send No Sound)  
**Last Updated**: December 30, 2025  
**Platform**: ESP32 (Crabik SLot ESP32-S3) with M5Stack Dial  
**Maintained by**: mawalton910