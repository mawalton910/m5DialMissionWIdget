#pragma once
#include <Arduino.h>
#include "secrets.h"

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "POIStory v26.4.1 "

// ===== OPERATIONAL MODES =====
// Device can operate in two modes, switchable via admin menu
enum OperationalMode {
    MODE_MISSION_WIDGET = 0,       // Shared NPC story mission using the assigned tag
    MODE_RELAY = 1,                // Preserve the original persisted Relay value
    MODE_STORY_MISSION_WIDGET = 2  // Manual mission-token scan compatibility mode
};

// Set true for a firmware build dedicated to the shared NPC story-round dial.
// False preserves the current local/random mission behavior.
const bool STORY_MODE_ON_BOOT = false;

// ===== ADMIN CONFIGURATION =====
#define ADMIN_BUTTON_HOLD_TIME 10000     // Hold button for 10 seconds to enter admin
#define ADMIN_EXIT_TIMEOUT 30000        // Auto-exit after 30s inactivity
#define ADMIN_MENU_ITEMS 9              // Device Info, Mission, Story Round, Relay, Dev Mode, Log, Scan Tag, NPC Tag, Exit
#define ADMIN_MENU_SCROLL_DELAY 100     // Scroll delay in ms

// ===== DEVICE CONFIGURATION =====
// Using values from secrets.h
// const String DEVICE_SERIAL and DEVICE_MAC defined in secrets.h

// ===== WIFI CONFIGURATION =====
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
const int WIFI_CONNECT_TIMEOUT = 10000; // ms
const int WIFI_RETRY_DELAY = 500; // ms

// ===== SERIAL LOGGING =====
enum SerialLogLevel {
  SERIAL_LOG_BASIC = 0,
  SERIAL_LOG_VERBOSE = 1,
  SERIAL_LOG_FULL = 2
};

// Increase/decrease this to control runtime serial verbosity globally.
const int SERIAL_LOG_LEVEL = SERIAL_LOG_FULL;

// ===== API CONFIGURATION =====
// Mission Mode endpoints
const String API_ENDPOINT = String(MISSION_API_ENDPOINT);

// Completion payload mode toggle:
// false = legacy payload (no game_id)
// true  = new payload (includes game_id)
const bool COMPLETION_INCLUDE_GAME_ID = false;

const int HTTP_TIMEOUT = 5000; // ms
const int MAX_HTTP_RETRIES = 3;

// ===== MISSION TIMEOUT CONFIGURATION =====
const int MISSION_TIMEOUT_MIN = 15;  // Configurable: mission duration in minutes
const unsigned long MISSION_TIMEOUT_MS = (unsigned long)MISSION_TIMEOUT_MIN * 60UL * 1000UL;

// ===== GAME DIFFICULTY =====
// Selected by players on the difficulty screen (after badge confirmation).
// Controls location count:  EASY=1, MEDIUM=2, HARD=3, EXTREME=4
// Also controls Safe Cracker visual mode: EASY→SC_EASY, MEDIUM/HARD→SC_NORMAL, EXTREME→SC_EXTREME
enum GameDifficulty { GAME_EASY = 0, GAME_MEDIUM, GAME_HARD, GAME_EXTREME };

// ===== BADGE COOLDOWN CONFIGURATION =====
const int BADGE_COOLDOWN_MIN = 5;  // Minutes before a completed badge can be reused
const unsigned long BADGE_COOLDOWN_MS = (unsigned long)BADGE_COOLDOWN_MIN * 60UL * 1000UL;

// ===== WIFI CONFIG TAGS =====
// Special tags that trigger WiFi configuration mode
// Format: First tag triggers config mode, subsequent tags contain SSID and password
extern const char* WIFI_CONFIG_TAGS[];
extern const int NUM_WIFI_CONFIG_TAGS;

// ===== WIFI PRESETS (Lookup Table) =====
// Maps NFC tag UIDs to human-readable WiFi credentials
// To add a preset: Scan tag in admin mode to get UID, then add entry here
struct WiFiCredential {
    const char* tagUID;      // NFC tag UID (space-delimited hex)
    const char* ssid;        // Human-readable WiFi SSID
    const char* password;    // Human-readable WiFi password
};

extern const WiFiCredential WIFI_PRESETS[];
extern const int NUM_WIFI_PRESETS;

// ===== DISPLAY CONFIGURATION ====
const int ERROR_DISPLAY_TIME = 2000; // ms
const int SUCCESS_DISPLAY_TIME = 2000; // ms

// ===== DISPLAY TEXT (centralized UI copy) =====
namespace DisplayText {
  // Usage reference (function names in mission_M5.ino)
  // Splash: SPLASH_LINE1/2, SPLASH_PRODUCT, SPLASH_VERSION -> displaySplashScreen()
  // Errors: BADGE_USED -> WAIT_FOR_BADGE branch; SAME_SESSION/USE_DIFFERENT_MISSION -> WAIT_FOR_MISSION_CARD reuse guard
  // Errors: INVALID_MISSION_CARD -> WAIT_FOR_MISSION_CARD invalid tag; VISIT_LOCATIONS_FIRST -> completion tag guard
  // Networking errors: WIFI_FAILED -> sendCombinedCompletionRequest()
  // State restore/reset: RESUMING_MISSION -> badge-restore flow; BADGE_RESET -> resetBadgeForReuse(); FULL_RESET/MEMORY_CLEARED/RESETTING -> fullReset()/reset()
  // Mission prompts: TAP_TO_COMPLETE/REWARD_LABEL -> update() completion prompt; TIMEOUT_TITLE/TIMEOUT_ACTION -> timeout hold screen; COMPLETION_TITLE/COMPLETION_SUBTITLE -> locked-difficulty prompt
  // Timer overlay: TIMER_TITLE/TIMER_HURRY -> update() timer overlay sprite
  // Log viewer: LOG_TITLE/LOG_FOOTER -> displayLogFile()
  // Success: MISSION_PASSED/RESPECT_PLUS -> drawMissionPassedScreen()
  // Networking status: SENDING_TO_SERVER -> sendCombinedCompletionRequest() send animation
  // Admin: ADMIN_EXIT -> displayAdminMode(); ADMIN_DEV_MODE_ON/OFF -> admin dev mode toggle
    // Splash screen
    constexpr char SPLASH_LINE1[] = "GURU";                 // Splash screen top line
    constexpr char SPLASH_LINE2[] = "GAMES";                // Splash screen bottom line
    constexpr char SPLASH_PRODUCT[] = "GURU GAMES";     // Splash product name
    constexpr char SPLASH_VERSION[] = "MISSION WIDGET";                 // Splash version label

    // WiFi configuration
    constexpr char WIFI_CONFIG_MODE[] = "WiFi Config Mode";     // WiFi config mode title
    constexpr char WIFI_SCAN_SSID[] = "Scan WiFi Preset Tag";   // Prompt for WiFi preset tag (lookup table)
    constexpr char WIFI_SAVED[] = "WiFi Saved";                 // Success message

    // Errors and warnings
    constexpr char BADGE_USED[] = "Badge Used";                  // Badge already used error
    constexpr char SAME_SESSION[] = "Same Session";              // Consecutive session block
    constexpr char USE_DIFFERENT_MISSION[] = "Use New Mission"; // Mission reuse warning
    constexpr char INVALID_MISSION_CARD[] = "Invalid Mission Card";   // Invalid mission card
    constexpr char VISIT_LOCATIONS_FIRST[] = "Visit 1+ Locations";    // Require progress warning
    constexpr char WIFI_FAILED[] = "WiFi Failed";                    // WiFi failure

    // Session state transitions
    constexpr char RESUMING_MISSION[] = "Resuming Mission";       // Restore mission after reset
    constexpr char BADGE_RESET[] = "Badge Reset";                 // Badge reset info
    constexpr char FULL_RESET[] = "FULL RESET";                   // Full reset notice
    constexpr char MEMORY_CLEARED[] = "Memory Cleared";           // After full reset
    constexpr char RESETTING[] = "Resetting...";                  // Generic reset progress

    // Mission flow prompts
    constexpr char TAP_TO_COMPLETE[] = "Tap to Complete";     // Completion prompt header
    constexpr char REWARD_LABEL[] = "Reward:";                // Reward label
    constexpr char TIMEOUT_TITLE[] = "TIME OUT";              // Timeout title
    constexpr char TIMEOUT_ACTION[] = "Scan Completion";       // Timeout action line
    constexpr char COMPLETION_TITLE[] = "SCAN";               // Completion title when locked
    constexpr char COMPLETION_SUBTITLE[] = "MISSION COMPLETION"; // Completion subtitle when locked

    // Timer overlay
    constexpr char TIMER_TITLE[] = "TIME LEFT";               // Timer overlay header
    constexpr char TIMER_HURRY[] = "HURRY!";                  // Timer critical warning

    // Log viewer
    constexpr char LOG_TITLE[] = "=== MISSION LOG ===";       // Log viewer title
    constexpr char LOG_FOOTER[] = "(Press button to close)";  // Log viewer footer

    // Success / reward visuals
    constexpr char MISSION_PASSED[] = "MISSION PASSED!";      // Success screen title
    constexpr char RESPECT_PLUS[] = "RESPECT +";              // Success screen subtitle

    // Networking
    constexpr char SENDING_TO_SERVER[] = "Sending to Server!"; // Send animation

    // Admin mode
    constexpr char ADMIN_EXIT[] = "(Hold 3s to go back)";       // Admin navigation instruction
    constexpr char ADMIN_DEV_MODE_ON[] = "DEV MODE: ON";        // Dev mode enabled
    constexpr char ADMIN_DEV_MODE_OFF[] = "DEV MODE: OFF";      // Dev mode disabled

    // Helpers
    inline String rewardLine(const String& difficulty) {
        return String(REWARD_LABEL) + " " + difficulty;
    }
}

// ===== COLORS (RGB565 format for TFT display) =====
const uint16_t COLOR_ERROR = 0xF800;      // Red
const uint16_t COLOR_SUCCESS = 0x07E0;    // Green
const uint16_t COLOR_WARNING = 0xFD20;    // Orange
const uint16_t COLOR_INFO = 0x8410;       // Gray
const uint16_t COLOR_PROCESSING = 0x001F; // Blue
const uint16_t COLOR_NONE = 0x000C;       // Dark Blue
const uint16_t COLOR_EXTREME = 0x8010;    // Purple

// ===== RESOURCE COLORS (RGB565 format for TFT display) =====
// Use these for text highlighting - convert RGB888 to RGB565
const uint16_t RESOURCE_COLOR_WEAPONS = 0xF460;    // Red (R:31, G:0, B:0)
const uint16_t RESOURCE_COLOR_SECURITY = 0x07FF;   // Yellow (R:31, G:63, B:0)
const uint16_t RESOURCE_COLOR_VEHICLE = 0xFFE0;  // White
const uint16_t RESOURCE_COLOR_MONEY = 0x07EF;      // Green (R:0, G:63, B:0)

// ===== RFID TAG CONFIGURATION =====
// Mission complete tags
const String COMPLETE_TAGS[] = {
    " EF 96 E2 64",
    " 6F 14 E4 64",
    " 04 F5 E8 2A 0A 12 90",
    " 04 2E D6 2A 0A 12 91",
    " 1D 51 BD 02 81 00 00",
    " 1D 5A D4 02 81 00 00",
    " 51 54 9F 3A",
    " 91 FE AB 3A",
    " 11 2E AB 3A",
    " 41 19 AE 3A",
    " 61 AD A5 3A",
    " 21 02 AE 3A"
};
const int NUM_COMPLETE_TAGS = sizeof(COMPLETE_TAGS) / sizeof(COMPLETE_TAGS[0]);

// Full reset tags (clears all data including completed badges)
const String FULL_RESET_TAGS[] = {
    " 3F 09 E5 64", " EF 23 18 64", " 04 20 83 D8 5F 61 80", " 0F A3 1A 64", " 9B CE 40 55",
};
const int NUM_FULL_RESET_TAGS = sizeof(FULL_RESET_TAGS) / sizeof(FULL_RESET_TAGS[0]);

// Regular reset tag
const String RESET_TAG[] = {
  " EF 23 18 99",
  " 1D A4 DD 02 81 00 00",
  " 1D E3 B9 03 81 00 00",
  " 04 D6 46 2A 0A 12 90",
  " 04 B3 45 2A 0A 12 90",
  " 04 F6 64 14 4E 61 80",
  " 04 C7 BD BB 2E 61 80",
  " 04 A5 24 15 4E 61 81",
  " 04 43 5B 14 4E 61 80",
  " 04 73 04 14 4E 61 80",
  " 04 29 10 14 4E 61 80"
};
const int NUM_RESET_TAGS = sizeof(RESET_TAG) / sizeof(RESET_TAG[0]);

// Badge reset tags (allows re-scanning of badge)
const String BADGE_RESET_TAGS[] = {
    " 04 49 53 CA 02 11 90", " 04 46 45 2A 0A 12 90"  // Replace with your actual badge reset tags
};
const int NUM_BADGE_RESET_TAGS = sizeof(BADGE_RESET_TAGS) / sizeof(BADGE_RESET_TAGS[0]);

// Scrub mission tags (marks badges on cooldown, clears session, no server call)
const String SCRUB_MISSION_TAGS[] = {
    " AF 74 D3 64"
};
const int NUM_SCRUB_MISSION_TAGS = sizeof(SCRUB_MISSION_TAGS) / sizeof(SCRUB_MISSION_TAGS[0]);

// Admin badge tags (enters admin mode for tag inspection)
const String ADMIN_BADGE_TAGS[] = {
  " 23 85 9F FE",
  " BF 1D 2A 40",
  " 4F 51 D2 64",
  " 43 D6 AC FE",
  " CD C0 AA 82",
  " F3 5D 25 03",
  " 13 F0 2F 04",
  " D1 AC 67 A9",
  " D3 57 1F FF",
  " 1F 69 D9 64",
  " 63 10 2F FF",
  " CB F5 15 55",
  " 04 12 07 D7 5F 61 80",
  " 6E 64 59 5B"
  // Add more admin badge UIDs here, one per line.
  // Example: " AA BB CC DD",
};
const int NUM_ADMIN_BADGE_TAGS = sizeof(ADMIN_BADGE_TAGS) / sizeof(ADMIN_BADGE_TAGS[0]);

// Mission reset tags (allows re-scanning of mission card)
// Mission reset tags removed — not used in current flow

// (No cooldown tags - feature removed)

// ===== RESOURCE CONFIGURATION =====
// Mission resource types (each location can have these)
enum ResourceType {
    RESOURCE_WEAPONS = 0,
    RESOURCE_SECURITY,
    RESOURCE_VEHICLE,
    RESOURCE_MONEY,
    RESOURCE_COUNT
};

// Status icon types for loud-mode UI feedback
enum StatusIconType {
  ICON_SUCCESS,
  ICON_ERROR,
  ICON_WARNING,
  ICON_INFO
};

// ===== DEVELOPMENT MODE =====
extern bool devMode;  // When true, all missions use GURU_HOME only

// ===== LOCATION CONFIGURATION =====

// ========================================
// EASY EDIT: Location Names (25 total)
// ========================================
// Just change the names here - keep the same order!
// Indexes 0-23 are production locations (alphabetical)
// Index 24 is GURU_HOME (dev location)
// NOTE: Actual definition is in POIStoryDualSendNoSoundv4.ino
extern const char* LOCATION_NAMES[];

// ========================================
// Location Structure (RFID Tags)
// ========================================
struct LocationInfo {
    const char* name;
    const String weaponTags[3];      // 3 RFID tags for WEAPONS resource
    const String securityTags[3];    // 3 RFID tags for SECURITY resource
    const String vehicleTags[3];     // 3 RFID tags for VEHICLE resource
    const String moneyTags[3];       // 3 RFID tags for MONEY resource
};

// Define your 24 production locations + GURU_HOME (dev location)
// NOTE: Each location needs 12 RFID tags (3 per resource type: WEAPONS, SECURITY, VEHICLE, MONEY)
// NOTE: Location names are pulled from LOCATION_NAMES[] array above for easy editing
// NOTE: Use empty strings "" for unused tag slots (e.g., if you only have 1-2 tags per resource)
const LocationInfo POI_LOCATIONS[] = {
    { LOCATION_NAMES[0],  // Allens Corner Store // Belle Isle Outpost
      {" DF 6D E0 64", "", ""},
      {" 52 D3 BB 29", "", ""},
      {" 1D BE 2E 03 81 00 00", "", ""},
      {" 1D 6C A1 02 81 00 00", "", ""} },
    { LOCATION_NAMES[1],  // Grizzly Gas // Brush Park
      {" 43 76 E7 02", "", ""},
      {" 91 E3 81 9F", "", ""},
      {" A1 F6 80 9F", "", ""},
      {" 73 63 82 9F", "", ""} },
    { LOCATION_NAMES[2],  // Loki-Eleven // Campus Martius
      {" E3 CB 1D FF", "", ""},
      {" 25 B6 80 9F", "", ""},
      {" D7 0C 82 9F", "", ""},
      {" 75 20 82 9F", "", ""} },
    { LOCATION_NAMES[3],  // PredStop // Capital Hill
      {" 5F 61 E6 64", "", ""},
      {" 5C E7 19 DA", "", ""},
      {" E2 FC 80 9F", "", ""},
      {" 8C 23 83 9F", "", ""} },
    { LOCATION_NAMES[4],  // Wicks Country Fuels // Dequindre Greenway
      {" 53 0F BE FE", "", ""},
      {" 04 69 93 2A 0A 12 90", "", ""},
      {" 1D D1 D0 02 81 00 00", "", ""},
      {" 1D 2C 95 02 81 00 00", "", ""} },
    { LOCATION_NAMES[5],  // Overbay Autos // Eastern Market
      {" 04 AD 70 D5 5F 61 80", "", ""},
      {" 92 47 81 9F", "", ""},
      {" 94 9A 81 9F", "", ""},
      {" E2 C7 80 9F", "", ""} },
    { LOCATION_NAMES[6],  // Bank of Angry Gremlin // Fenkell Frontlines
      {" 04 39 48 D8 5F 61 80", "", ""},
      {" 83 C1 C0 FE", "", ""},
      {" 1D 0F DA 02 81 00 00", "", ""},
      {" 1D 0C ED 02 81 00 00", "", ""} },
    { LOCATION_NAMES[7],  // Rookie Country Club // Fisher Beacon
      {" 6F B6 CA 64", "", ""},
      {" 43 01 FC 02", "", ""},
      {" 1D 04 30 03 81 00 00", "", ""},
      {" 1D 57 FC 02 81 00 00", "", ""} },
    { LOCATION_NAMES[8],  // The Nella Lounge // Greektown Alleyways
      {" 04 00 36 D5 5F 61 80", "", ""},
      {" 1D 36 1E 03 81 00 00", "", ""},
      {" 1D 3C A8 02 81 00 00", "", ""},
      {" 1D 76 A1 03 81 00 00", "", ""} },
    { LOCATION_NAMES[9],  // Capital Hill Private Security // Guardian Plaza
      {" 1D E4 3D 03 81 00 00", "", ""},
      {" E3 38 1B FF", "", ""},
      {" 1D 20 44 03 81 00 00", "", ""},
      {" 1D 33 A0 03 81 00 00", "", ""} },
    { LOCATION_NAMES[10],  // Stayback Mine // Iron Depot
      {" B3 58 95 0F", "", ""},
      {" C3 8B E6 02", "", ""},
      {" 1D 56 E9 02 81 00 00", "", ""},
      {" 1D CB E7 02 81 00 00", "", ""} },
    { LOCATION_NAMES[11],  // Saint Lawrence Cathedral // Masonic Stronghold
      {" 9F 32 23 64", "", ""},
      {" 04 18 49 2A 0A 12 91", "", ""},
      {" 1D 50 DA 02 81 00 00", "", ""},
      {" 1D BE BB 03 81 00 00", "", ""} },
    { LOCATION_NAMES[12],  // Armageddon Power Station // Midtown Commons
      {" 43 54 60 13", "", ""},
      {" EE 2A 81 9F", "", ""},
      {" F7 AE 81 9F", "", ""},
      {" E6 02 83 9F", "", ""} },
    { LOCATION_NAMES[13],  // DNR Hospital // Motown Archives
      {" E3 3A F3 02", "", ""},
      {" 04 57 75 D5 5F 61 80", "", ""},
      {" 1D 5B 94 02 81 00 00", "", ""},
      {" 1D 32 2F 03 81 00 00", "", ""} },
    { LOCATION_NAMES[14],  // A.O.A.T. Army Surplus // New Center
      {" 13 C1 E8 02", "", ""},
      {" F4 E1 80 9F", "", ""},
      {" 6A 18 82 9F", "", ""},
      {" CA AF 82 9F", "", ""} },
    { LOCATION_NAMES[15],  // BUT DID YOU STRIKE // Old Fort Bastion
      {" 3F 87 DA 64", "", ""},
      {" 1F 63 1B 64", "", ""},
      {" 1D B3 F5 02 81 00 00", "", ""},
      {" 1D 6E 97 02 81 00 00", "", ""} },
    { LOCATION_NAMES[16],  // Princess Substation 7 // Packard Ruins
      {" 04 45 C0 2A 0A 12 90", "", ""},
      {" 73 F5 E8 02", "", ""},
      {" 1D C6 81 03 81 00 00", "", ""},
      {" 1D AC F0 02 81 00 00", "", ""} },
    { LOCATION_NAMES[17],  // Turf War Arena // Renaissance Hub
      {" 6F 3B D5 64", "", ""},
      {" 3F D6 CD 64", "", ""},
      {" 1D 7F 3A 03 81 00 00", "", ""},
      {" 1D A3 99 03 81 00 00", "", ""} },
    { LOCATION_NAMES[18],  // Gatormaille Crossing Prawn Shop // Riverfront Plaza
      {" EF 6A F4 64", "", ""},
      {" 1B 90 81 9F", "", ""},
      {" 44 56 81 9F", "", ""},
      {" 2B EB 82 9F", "", ""} },
    { LOCATION_NAMES[19],  // SHEro's Taproom // Spirits Watch
      {" 23 D1 74 05", "", ""},
      {" AF 7A 1F 64", "", ""},
      {" 1D 5C ED 02 81 00 00", "", ""},
      {" 1D 65 A7 03 81 00 00", "", ""} },
    { LOCATION_NAMES[20],  // Hall of High Scores // TechTown
      {" 33 23 66 E8", "", ""},
      {" 05 18 81 9F", "", ""},
      {" 66 F2 80 9F", "", ""},
      {" 79 CB 81 9F", "", ""} },
    { LOCATION_NAMES[21],  // Meadman Pulse Tower // Tower of Pages
      {" 04 51 58 2A 0A 12 91", "", ""},
      {" A3 DF EA 02", "", ""},
      {" 1D D3 D0 02 81 00 00", "", ""},
      {" 1D 60 C4 02 81 00 00", "", ""} },
    { LOCATION_NAMES[22],  // GGs Inc. // Waters Edge
      {" 04 5D 5A D6 5F 61 80", "", ""},
      {" F3 64 B4 FE", "", ""},
      {" 1D 6B DA 02 81 00 00", "", ""},
      {" 1D 69 AC 02 81 00 00", "", ""} },
    { LOCATION_NAMES[23],  // Laceys Lounge // West Village
      {" FF BC D4 64", "", ""},
      {" 55 64 82 9F", "", ""},
      {" F7 29 81 9F", "", ""},
      {" B3 4C 81 9F", "", ""} },
    // === DEV LOCATION ===
    { LOCATION_NAMES[24],  // GURU_HOME
      {" FF 38 DC 64", " 04 F2 D2 7D CC 2A 81", ""},
      {" 53 FC 04 03", " 04 74 D3 7F CC 2A 81", ""},
      {" F3 21 E9 02", " 04 8C F2 7F CC 2A 81", ""},
      {" EF B2 D7 64", " 86 10 EA AD", ""} }
};

const int TOTAL_LOCATIONS = sizeof(POI_LOCATIONS) / sizeof(POI_LOCATIONS[0]);
const int REQUIRED_LOCATIONS = 4; // Exactly 4 mission spots per session

// Mission card tags - ALL cards trigger HEIST missions (category selection removed)
// Add your physical mission card RFID tags here
const String MISSION_CARD_TAGS[] = {
    " EE 57 67 5B", " 3E BB 01 0C",
  " 4F 74 CA 64", " B3 DC 00 03", " 1F 59 D2 64",
  " CE D5 0E 0C", " 6F 8F 17 64",
    // Add more mission card tags below as needed
    " 04 81 14 2A 0A 12 90", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", ""
};
const int NUM_MISSION_CARD_TAGS = sizeof(MISSION_CARD_TAGS) / sizeof(MISSION_CARD_TAGS[0]);

// ===== FREE ROAM MISSION CARD TAGS =====
// Free Roam missions: 4 random POIs are selected.  Players visit any subset and scan
// any resource they choose at each POI.  Each resource TYPE can only be collected once.
// Difficulty is determined by how many POIs are visited before scanning the completion card.
const String FREE_ROAM_MISSION_TAGS[] = {
    " B3 DC 00 03",  // FREE ROAM card 1
    // Add more Free Roam cards here as needed
};
const int NUM_FREE_ROAM_MISSION_TAGS = sizeof(FREE_ROAM_MISSION_TAGS) / sizeof(FREE_ROAM_MISSION_TAGS[0]);

// Validate location tags - for debugging
inline bool validateTags() {
    // Validate location resource tags (12 per location: 3 tags each for WEAPONS, SECURITY, VEHICLE, MONEY)
    for (int i = 0; i < TOTAL_LOCATIONS; i++) {
        const LocationInfo& loc = POI_LOCATIONS[i];
        
        // Check all 4 resource types, each with 3 tag slots
        const String* tagArrays[4] = { loc.weaponTags, loc.securityTags, loc.vehicleTags, loc.moneyTags };
        const char* resourceNames[4] = { "WEAPONS", "SECURITY", "VEHICLE", "MONEY" };
        
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 3; k++) {
                const String& tag = tagArrays[j][k];
                if (tag.isEmpty()) continue; // Skip empty tag slots
                
                if (tag[0] != ' ') {
                    Serial.println("ERROR: Invalid " + String(resourceNames[j]) + " tag " + String(k+1) + " format at location " + String(i) + " (" + String(loc.name) + ") - must start with space");
                    return false;
                }
                if (tag.length() < 6 || tag.length() > 25) {
                    Serial.println("ERROR: Invalid " + String(resourceNames[j]) + " tag " + String(k+1) + " length at location " + String(i) + " (" + String(loc.name) + ")");
                    return false;
                }
            }
        }
    }
    return true;
}
