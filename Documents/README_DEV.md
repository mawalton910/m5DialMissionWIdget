# Guru Games - Mission - POI Story System
**Current Version**: POIStory v25.8.1

A location-based mission tracking system built on the M5Stack Dial (ESP32-S3) for immersive story-driven gameplay in live-action role-playing events.

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

## 📦 Features

### Mission System
- **Story Generation**: Dynamic narrative-driven missions with category-specific phrases
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

### Administrative Features
- **Device Info**: View serial number, MAC address, firmware version, current mode, and WiFi status with IP
- **Mode Switching**: Toggle between Mission Widget, Buy Station, and Relay modes
- **Mission Log Viewer**: Review all mission events with timestamps (press BtnA in badge-wait state)
- **Integrated Modes**: Buy Station for loot claiming, Relay for badge updates

### Over-the-Air (OTA) Updates
- **Remote firmware updates** via GitHub
- Triggered by scanning designated RFID tag
- Automatic download and installation
- Progress display with percentage and download stats
- Automatic reboot after successful update

## 🚀 Getting Started

### Prerequisites

1. **Arduino IDE** with ESP32 board support
2. **M5Dial Library**: Install via Arduino Library Manager
3. **Required Libraries**:
   - WiFi (ESP32 core)
   - HTTPClient
   - ArduinoJson
   - WiFiClientSecure
   - HTTPUpdate

### Installation

1. Configure your settings in `mission_M5.ino`:
   ```cpp
   const char* WIFI_SSID = "YourWiFiSSID";
   const char* WIFI_PASSWORD = "YourWiFiPassword";
   ```

2. Update device identifiers in `Config.h`:
   ```cpp
   const char* DEVICE_SERIAL = "your_device_serial";
   const char* DEVICE_MAC = "your_mac_address";
   ```

3. Update API endpoints in `Config.h` if needed

4. Select **Crabik SLot ESP32-S3** as your board

5. Upload to your M5Stack Dial device

## 📡 OTA Updates

### Triggering an Update

1. Scan designated OTA trigger RFID card
2. Device connects to OTA WiFi (can be different from normal WiFi)
3. Downloads firmware from: `https://raw.githubusercontent.com/[USER]/[REPO]/main/[FIRMWARE].bin`
4. Displays progress with percentage and download size
5. Installs and reboots automatically

### Deploying New Firmware

1. **Export Compiled Binary**:
   - Arduino IDE → Sketch → Export Compiled Binary
   
2. **Rename the file** to: `your_firmware.ino.bin` (must match FIRMWARE_FILENAME in OTAUpdate.h)

3. **Upload to GitHub**:
   - Place file in the **root** of the `main` branch
   - Commit and push changes

4. **Deploy**:
   - Scan the OTA trigger card on any device
   - All devices will update to the new version

### OTA Configuration

Configure in `OTAUpdate.h`:
```cpp
// OTA Trigger Card (scan to start update)
const String OTA_TRIGGER_UIDS[] = {
    "YOUR_OTA_TAG_UID",  // Add more trigger cards as needed
};

// Update WiFi (can be different from normal WiFi)
const char* OTA_WIFI_SSID = "YourOTAWiFi";
const char* OTA_WIFI_PASSWORD = "YourOTAPassword";

// GitHub Settings
const char* GITHUB_USER = "your_github_username";
const char* GITHUB_REPO = "your_repository_name";
const char* FIRMWARE_FILENAME = "your_firmware.ino.bin";
String targetBranch = "main";
```

## 🎯 Usage

### Mission Workflow

1. **Scan Player Badge** → System identifies player and checks history
2. **Scan Mission Card** → Select category (prevents consecutive use)
3. **Story Generation** → Device creates 4 random location mission
4. **Navigate Story** → Use rotary encoder to scroll through pages
5. **Visit Locations** → Scan location RFID tags at real-world sites
6. **Track Progress** → View timer with BtnA during mission
7. **Complete Mission** → Scan completion card when ready
8. **Server Submission** → Device sends mission data and respect reward

### Special Cards

**Control Cards:**
- **Badge Reset Card**: Save mission state, prompt for new badge
- **Full Reset Card**: Clear all state, history, and logs
- **Completion Cards**: Trigger mission submission (8 variants)
- **Mission Cards**: Start new mission in specific category (8 categories)

**Location Tags:**
- 25 locations with 4 tags each (weaponTag, securityTag, vehicleTag, moneyTag)
- Tags are space-delimited hex format: `" AA BB CC DD"`
- Example: Each location has 4 different RFID tags, one for each resource type

### Admin Access

Scan admin badge to enter admin mode. Navigate using rotary encoder:
- **Rotate**: Scroll through menu
- **Press**: Select option
- **Scan Admin Badge Again**: Exit admin mode

Available admin options:
1. **Device Info**: View serial number, MAC address, firmware version (POIStory v25.7.0), current operational mode, and WiFi status with IP address
2. **Mission Widget Mode**: Switch to story mission tracking mode (default)
3. **Buy Station Mode**: Switch to player/loot transaction system for claiming items
4. **Relay Mode**: Switch to simple badge relay/update mode
5. **Exit Admin**: Return to current mode's initial state

**Navigation**: 
- Rotate encoder to scroll through options
- Press button to select
- Scan admin badge again at any time to exit completely

### Mission Timer

- **Default Duration**: 30 minutes (configurable in Config.h)
- **Display Timer**: Press BtnA during mission to see MM:SS countdown
- **Warning**: Red "HURRY!" appears when < 60 seconds remain
- **Timeout Behavior**:
  - No difficulty attained: Hold screen, badge-reset or completion clears
  - Difficulty attained: Locks difficulty, allows completion submission
Mission completion submission
- Respect reward calculation
- Badge history tracking
- Location verification
- Loot management (Buy Station mode)

**Primary API Endpoints:**
```cpp
const String API_ENDPOINT = "https://your-server.com/api/missionComplete";
const String REWARD_ENDPOINT = "https://your-server.com/api/rewardDistribution";
```

**Mission Data Submitted:**
- Player badge UUID
- Mission category (Heist, Police, etc.)
- Difficulty earned (Easy, Medium, Hard, Extreme)
- Respect reward amount
- Device identifiers (serial, MAC)

## 🐛 Troubleshooting

### WiFi Connection Issues
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check SSID and password in `mission_M5.ino`
- Use admin Device Info to check WiFi status and IP address

### OTA Update Failures
- **"WIFI FAILED"**: Check OTA WiFi credentials in `OTAUpdate.h`
- **"UPDATE FAILED"**: Verify `.bin` file exists at GitHub URL
- **"NO UPDATE AVAILABLE"**: Check branch name and filename
- Test URL in browser: `https://raw.githubusercontent.com/[USER]/[REPO]/main/[FIRMWARE].bin`

### RFID Read Issues
- Ensure badge is close to device (within 2-3 cm)
- Check Serial Monitor (115200 baud) for UID readings
- Location tags require exact match from Config.h arrays
- Allow 1-second debounce between scans

### Mission Issues
- **"Cannot Complete Mission"**: Check if at least 1 location visited
- **"Mission Timeout"**: If no progress, scan badge-reset or completion card
- **Consecutive Use Block**: Different player must complete between same badge+mission pairs
- **Badge-Reset State**: New badge scan restores previous mission state



## 🗺️ Location Management

Locations are centralized in `LOCATION_NAMES[]` array:
```cpp
const char* LOCATION_NAMES[] = {
    "Belle Isle Outpost",    // Index 0
    "Brush Park",            // Index 1
    // ... 22 more locations
    "Guru Home"              // Index 24 - DEV LOCATION
};
```

**To edit location names**: Simply change the string in the array. The name updates throughout the entire codebase (story generation, admin mode, logs).

**To add new locations**:
1. Add name to `LOCATION_NAMES[]` array
2. Add corresponding entry to `POI_LOCATIONS` in Config.h
3. Define 4 unique RFID tags (weaponTag, securityTag, vehicleTag, moneyTag)

## 📊 Serial Monitor

Connect at **115200 baud** for debug output:
- WiFi connection status with IP address
- RFID card UIDs as they're scanned
- API request/response logs
- Mission state changes and location visits
- OTA update progress
- Error messages and diagnostics

## 🔄 Version Control

Current firmware version defined in `Config.h`:
```cpp
#define FIRMWARE_VERSION "POIStory v25.7.0"
```

Version history managed through GitHub releases. Use the OTA system to deploy updates to all devices simultaneously.

## 📝 License

[Add your license here]

## 👥 Contributors

- [Your Name/Team]

## 🤝 Contributing

[Add contribution guidelines if applicable]

## 📧 Support

For issues and questions:
- Open an issue on GitHub
- [Add contact information]

## 🙏 Acknowledgments

- Built with M5Stack Dial hardware
- ESP32-S3 platform
- Arduino framework
- M5Dial library by M5Stack

---

**Current Version**: POIStory v25.8.1
**Last Updated**: December 30, 2025  
**Platform**: ESP32-S3 (Crabik SLot ESP32-S3) with M5Stack Dial  
**Maintained by**: [Your Organization/Name]