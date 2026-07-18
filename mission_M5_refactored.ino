// ============================================================
// mission_M5.ino Ã¢â‚¬â€ Main orchestrator (refactored)
// ============================================================
// Upload with board: "Crabik SLot ESP32-S3"
//
// This is the top-level state machine that wires together all
// the extracted modules and drives the mission lifecycle.
//
// Module map:
//   Config.h          Ã¢â‚¬â€œ Hardware constants, timeouts, colours
//   MissionBase.h     Ã¢â‚¬â€œ Abstract mission interface
//   *Mission.h/.cpp   Ã¢â‚¬â€œ Concrete mission types
//   StateManager.h    Ã¢â‚¬â€œ NVS persistence
//   Logger.h          Ã¢â‚¬â€œ SPIFFS mission log
//   OTAUpdate.h       Ã¢â‚¬â€œ Over-the-air firmware update
//   Sounds.h          Ã¢â‚¬â€œ Audio tone definitions
// ============================================================

#include <Arduino.h>
#include <M5Dial.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <esp_log.h>
#include <esp_system.h>
#include "Config.h"
#include "MissionBase.h"
#include "FreeRoamMission.h"
#include "StateManager.h"
#include "Logger.h"
#include "OTAUpdate.h"
#include "GuruLogo.h"
#include "Sounds.h"
#include <unit_audioplayer.hpp>

// ===== External AudioPlayer (Unit AudioPlayer on Port B) =====
AudioPlayerUnit audioPlayer;
bool audioPlayerReady = false;

// ===== WiFi credentials (editable per deployment) =====
const char* WIFI_SSID     = DEFAULT_WIFI_SSID;
const char* WIFI_PASSWORD = DEFAULT_WIFI_PASSWORD;

// WiFi config tags
const char* WIFI_CONFIG_TAGS[] = {WIFI_CONFIG_TRIGGER_TAG};
const int NUM_WIFI_CONFIG_TAGS = sizeof(WIFI_CONFIG_TAGS) / sizeof(WIFI_CONFIG_TAGS[0]);

const WiFiCredential WIFI_PRESETS[] = {
  {WIFI_PRESET_DEFAULT_TAG, DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD},
  {WIFI_PRESET_ALT_TAG,     ALT_WIFI_SSID,     ALT_WIFI_PASSWORD},
};
const int NUM_WIFI_PRESETS = sizeof(WIFI_PRESETS) / sizeof(WIFI_PRESETS[0]);

// Location names (25: 24 production + 1 dev)
const char* LOCATION_NAMES[] = {
    "Allens Corner Store",              // 0
    "Grizzly Gas",                      // 1
    "Loki-Eleven",                      // 2
    "PredStop",                         // 3
    "Wicks Country Fuels",              // 4
    "Overbay Autos",                    // 5
    "Bank of Angry Gremlin",            // 6
    "Rookie Country Club",              // 7
    "The Nella Lounge",                 // 8
    "Capital Hill Private Security",    // 9
    "Stayback Mine",                    // 10
    "Saint Lawrence Cathedral",         // 11
    "Armageddon Power Station",         // 12
    "DNR Hospital",                     // 13
    "A.O.A.T. Army Surplus",            // 14
    "But Did You Not",                  // 15
    "Princess Substation 7",            // 16
    "Turf War Arena",                   // 17
    "Gatormaille Crossing Prawn Shop",  // 18
    "SHEro's Taproom",                  // 19
    "Hall of High Scores",              // 20
    "Meadman Pulse Tower",              // 21
    "GGs Inc.",                         // 22
    "Laceys Lounge",                    // 23
    "Guru Home"                         // 24 - DEV LOCATION
};

// Dev mode flag (global, accessible by missions)
bool devMode = false;

// ===== Audio feedback callbacks =====
void playSuccessTone()  { playSound(SND_BADGE_SCAN,     SND_BADGE_SCAN_LEN, 255, "BADGE_SCAN", EXT_BADGE_SCAN); }
void playErrorTone()    { playSound(SND_ERROR,           SND_ERROR_LEN, 255, "ERROR", EXT_ERROR); }
void playAcceptTone()   { playSound(SND_LOCATION_VISIT,  SND_LOCATION_VISIT_LEN, 255, "LOCATION_VISIT", EXT_LOCATION_VISIT); }
void playConfirmPlayersTone() { playSound(SND_CONFIRM_PLAYERS, SND_CONFIRM_PLAYERS_LEN, 255, "CONFIRM_PLAYERS", 0); }
void playConfirmSelectTone()  { playSound(SND_CONFIRM_SELECT,  SND_CONFIRM_SELECT_LEN, 255, "CONFIRM_SELECT", 0); }
void playAdminAcceptTone()    { playSound(SND_ADMIN_ACCEPT,    SND_ADMIN_ACCEPT_LEN, 255, "ADMIN_ACCEPT", 0); }
void playInfoTone()           { playSound(SND_INFO_CHIME,      SND_INFO_CHIME_LEN, 255, "INFO_CHIME", 0); }
void playStateChangeTone()    { playSound(SND_STATE_CHANGE,    SND_STATE_CHANGE_LEN, 255, "STATE_CHANGE", 0); }
void playRemoveTone()   { playSound(SND_BADGE_REMOVED,   SND_BADGE_REMOVED_LEN, 255, "BADGE_REMOVED", EXT_BADGE_REMOVED); }

// ===== Loud-mode visual effects =====
const bool LOUD_MODE = true;

static inline bool isVerboseLogging() { return SERIAL_LOG_LEVEL >= SERIAL_LOG_VERBOSE; }
static inline bool isFullLogging() { return SERIAL_LOG_LEVEL >= SERIAL_LOG_FULL; }

String wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "NO_SHIELD";
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID";
    case WL_SCAN_COMPLETED: return "SCAN_DONE";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

String extractHostFromUrl(const String& url) {
  int start = url.indexOf("//");
  start = (start >= 0) ? start + 2 : 0;
  int slash = url.indexOf('/', start);
  String hostPort = (slash >= 0) ? url.substring(start, slash) : url.substring(start);
  int colon = hostPort.indexOf(':');
  return (colon >= 0) ? hostPort.substring(0, colon) : hostPort;
}

void logLongSerial(const char* label, const String& value, size_t chunkSize = 512) {
  Serial.printf("%s length=%u\n", label, (unsigned)value.length());
  for (size_t offset = 0; offset < (size_t)value.length(); offset += chunkSize) {
    size_t end = offset + chunkSize;
    if (end > (size_t)value.length()) end = value.length();
    String chunk = value.substring(offset, end);
    Serial.printf("%s[%u..%u] %s\n", label, (unsigned)offset, (unsigned)end, chunk.c_str());
    delay(1);
  }
}


const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}
void logNetworkSnapshot(const char* stage, const String& endpoint = "") {
  if (!isVerboseLogging()) return;

  wl_status_t st = WiFi.status();
  Serial.printf("[NET] %lums  %s | status=%s(%d) ip=%s gw=%s mask=%s dns=%s rssi=%d heap=%u\n",
      millis(), stage, wifiStatusToString(st).c_str(), (int)st,
      WiFi.localIP().toString().c_str(),
      WiFi.gatewayIP().toString().c_str(),
      WiFi.subnetMask().toString().c_str(),
      WiFi.dnsIP().toString().c_str(),
      WiFi.RSSI(),
      (unsigned)ESP.getFreeHeap());

  if (!endpoint.isEmpty() && isFullLogging()) {
    String host = extractHostFromUrl(endpoint);
    IPAddress resolved;
    int dnsOk = WiFi.hostByName(host.c_str(), resolved);
    Serial.printf("[NET] %lums  DNS %s => %s (ok=%d)\n", millis(), host.c_str(), resolved.toString().c_str(), dnsOk);
  }
}

void flashScreen(uint16_t color) {
  if (!LOUD_MODE) { M5Dial.Lcd.fillScreen(color); return; }
  M5Dial.Lcd.fillScreen(TFT_WHITE);
  delay(35);
  M5Dial.Lcd.fillScreen(color);
}

void drawAlertBorder(uint16_t color) {
  if (!LOUD_MODE) return;
  int cx = M5Dial.Display.width() / 2;
  int cy = M5Dial.Display.height() / 2;
  int r  = min(cx, cy) - 1;
  for (int i = 0; i < 4; i++) M5Dial.Display.drawCircle(cx, cy, r - i, color);
}

void drawStatusIcon(StatusIconType type) {
  if (!LOUD_MODE) return;
  int x = M5Dial.Display.width() - 22, y = 22;
  uint16_t bg = TFT_BLUE; String glyph = "i";
  switch (type) {
    case ICON_SUCCESS: bg = TFT_GREEN;  glyph = "OK"; break;
    case ICON_ERROR:   bg = TFT_RED;    glyph = "X";  break;
    case ICON_WARNING: bg = TFT_ORANGE; glyph = "!";  break;
    default: break;
  }
  M5Dial.Display.fillCircle(x, y, 18, bg);
  M5Dial.Display.setTextDatum(MC_DATUM);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.setTextColor(TFT_WHITE);
  M5Dial.Display.drawString(glyph, x, y - 1);
}

// ===== State machine =====
enum TrackerState : int;
const char* stateName(TrackerState s);

enum TrackerState : int {
  WAIT_FOR_BADGE,
  WAIT_FOR_MISSION_CARD,
  RUN_MISSION,
  ADMIN_MODE,
  WIFI_CONFIG,
  CONFIRM_PLAYERS,
  RELAY_WAIT_BADGE,
  WAIT_FOR_NPC_TOKEN
};

const char* stateName(TrackerState s) {
  switch (s) {
    case WAIT_FOR_NPC_TOKEN:   return "WAIT_FOR_NPC_TOKEN";
    case WAIT_FOR_BADGE:       return "WAIT_FOR_BADGE";
    case WAIT_FOR_MISSION_CARD:return "WAIT_FOR_MISSION_CARD";
    case RUN_MISSION:          return "RUN_MISSION";
    case ADMIN_MODE:           return "ADMIN_MODE";
    case WIFI_CONFIG:          return "WIFI_CONFIG";
    case CONFIRM_PLAYERS:      return "CONFIRM_PLAYERS";
    case RELAY_WAIT_BADGE:     return "RELAY_WAIT_BADGE";
    default:                   return "UNKNOWN";
  }
}

// ============================================================
// StoryTracker Ã¢â‚¬â€ main application class
// ============================================================
class StoryTracker {
public:
  // --- Public sub-systems (available to loop()) ---
  StateManager stateManager;
  Logger       logger;
  TrackerState trackerState    = WAIT_FOR_BADGE;
  OperationalMode currentMode = MODE_MISSION_WIDGET;
  // Admin state
  int  adminMenuSelection = 0;
  bool adminInMenu        = true;
  String adminPropUID;

  StoryTracker() {}

  // --- Getters / setters for external access ---
  int  getPlayerCount() const { return playerCount; }
  void setPlayerCount(int c)  { playerCount = c; }
  String getPlayerUUID(int i) const { return playerUUIDs[i]; }
  void   setPlayerUUID(int i, const String& u) { playerUUIDs[i] = u; }
  String getCurrentBadgeUID() const { return currentBadgeUID; }
  void   setCurrentBadgeUID(const String& u) { currentBadgeUID = u; }
  String getCurrentMissionCardUID() const { return currentMissionCardUID; }
  void   setCurrentMissionCardUID(const String& u) { currentMissionCardUID = u; }
  MissionBase* getCurrentMission() const { return currentMission; }
  void setCurrentMission(MissionBase* m) { currentMission = m; }
  unsigned long getMissionStartTime() const { return missionStartTime; }
  void setMissionStartTime(unsigned long ms) { missionStartTime = ms; }
  String getLockedDifficulty() const { return lockedDifficulty; }
  void setLockedDifficulty(const String& d) { lockedDifficulty = d; }
  void setLastCompletedMissionCardUID(const String& u) { lastCompletedMissionCardUID = u; }
  void setTrackerState(TrackerState s) { trackerState = s; }
  void setMissionLocked(bool v) { missionLocked = v; }
  void setAwaitingCompletionScan(bool v) { awaitingCompletionScan = v; }
  void setCompletionPromptShown(bool v) { completionPromptShown = v; }
  void setLastTimerUpdate(unsigned long v) { lastTimerUpdate = v; }
  void setRelayBadgeUID(const String& v) { relayBadgeUID = v; }
  void setAdminInMenu(bool v) { adminInMenu = v; }
  void setAdminMenuSelection(int v) { adminMenuSelection = v; }
  void setSavedTrackerState(int v) { savedTrackerState = v; }
  void setSavedMissionCardUID(const String& v) { savedMissionCardUID = v; }
  void setAwaitingBadgeRestore(bool v) { awaitingBadgeRestore = v; }

  // ---- Activity / screen timeout ----
  void bumpActivity() {
    lastActivityMs = millis();
    if (!screenOn) { screenOn = true; M5Dial.Display.setBrightness(128); }
  }

  // ---- Touch shortcut: advance from badge to confirm ----
  void advanceFromBadgeScreen() {
    if (trackerState == WAIT_FOR_BADGE && playerCount > 0) {
      confirmSelection  = 0;
      confirmEncoderPos = M5Dial.Encoder.read();
      trackerState      = CONFIRM_PLAYERS;
      displayConfirmPlayers();
    }
  }

  void applyConfirmPlayersSelection() {
    if (confirmSelection == 0) {
      Serial.printf("[ACTION] %lums  Players confirmed Ã¢â‚¬â€ starting mission\n", millis());
      if (currentMode == MODE_STORY_MISSION_WIDGET) {
        trackerState = WAIT_FOR_MISSION_CARD;
        displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
      } else {
        startFreeRoamFlow(true);
      }
    } else {
      Serial.printf("[ACTION] %lums  Add more players selected\n", millis());
      trackerState = WAIT_FOR_BADGE;
      displayPlayerRegistration();
    }
  }

  bool handleConfirmPlayersTouch(int x, int y) {
    if (trackerState != CONFIRM_PLAYERS) return false;

    int cx = M5Dial.Display.width() / 2;
    bool yesTap = (x >= cx - 55 && x <= cx + 55 && y >= 105 && y <= 141);
    bool noTap  = (x >= cx - 55 && x <= cx + 55 && y >= 153 && y <= 189);

    if (!yesTap && !noTap) return false;

    confirmSelection = yesTap ? 0 : 1;
    displayConfirmPlayers();
    playConfirmSelectTone();
    applyConfirmPlayersSelection();
    return true;
  }

  // ============================================================
  // begin() Ã¢â‚¬â€ one-time setup
  // ============================================================
  void begin() {
    Serial.printf("[BOOT] %lums  Firmware: %s\n", millis(), FIRMWARE_VERSION);
    Serial.printf("[BOOT] %lums  Serial: %s  MAC: %s\n", millis(), DEVICE_SERIAL_NUM.c_str(), DEVICE_MAC_ADDR.c_str());
    Serial.printf("[BOOT] before splash heap=%u\n", (unsigned)ESP.getFreeHeap());
    displaySplashScreen();
    Serial.printf("[BOOT] after splash heap=%u\n", (unsigned)ESP.getFreeHeap());
    Serial.printf("[BOOT] before stateManager.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
    stateManager.begin();
    Serial.printf("[BOOT] after stateManager.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
    Serial.printf("[BOOT] before logger.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
    logger.begin();
    Serial.printf("[BOOT] after logger.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
    storyNpcToken = normalizeRfidToken(stateManager.getStoryNpcToken());
    if (storyNpcToken.length() > 0 && storyNpcToken != stateManager.getStoryNpcToken()) {
      stateManager.setStoryNpcToken(storyNpcToken);
    }

    currentMode = (OperationalMode)stateManager.getOperationalMode();
    if (currentMode != MODE_MISSION_WIDGET && currentMode != MODE_STORY_MISSION_WIDGET && currentMode != MODE_RELAY) {
      currentMode = STORY_MODE_ON_BOOT ? MODE_STORY_MISSION_WIDGET : MODE_MISSION_WIDGET;
      stateManager.setOperationalMode(currentMode);
    }
    if (currentMode == MODE_RELAY) {
      connectToWiFi();
    } else {
      shutdownWiFi();
    }
    const char* modeLabel = currentMode == MODE_RELAY ? "Relay" : (currentMode == MODE_STORY_MISSION_WIDGET ? "Story Round" : "Mission Widget");
    Serial.printf("[BOOT] %lums  Mode: %s\n", millis(), modeLabel);
    logger.log("Boot: " + String(modeLabel));

    if (currentMode == MODE_MISSION_WIDGET || currentMode == MODE_STORY_MISSION_WIDGET) {
      trackerState = storyNpcToken.length() > 0 ? WAIT_FOR_BADGE : WAIT_FOR_NPC_TOKEN;
      // Wait on splash logo until boot audio finishes (10s from audio start)
      {
        const unsigned long BOOT_AUDIO_MS = 10000UL;
        unsigned long elapsed = millis() - _splashAudioStartMs;
        if (elapsed < BOOT_AUDIO_MS) {
          Serial.printf("[BOOT] Holding splash for %lums while boot audio plays\n", BOOT_AUDIO_MS - elapsed);
          delay(BOOT_AUDIO_MS - elapsed);
        }
      }
      if (trackerState == WAIT_FOR_NPC_TOKEN) {
        displayScanNpcToken();
        waitingForBadge = false;
      } else {
        displayScanBadge();
        waitingForBadge = true;
      }
    } else {
      trackerState = RELAY_WAIT_BADGE;
      displayRelayBadgePrompt();
    }
  }

  // ============================================================
  // restoreFromSavedSnapshot() Ã¢â‚¬â€ power-loss recovery
  // ============================================================
  bool restoreFromSavedSnapshot() {
    if ((currentMode == MODE_MISSION_WIDGET || currentMode == MODE_STORY_MISSION_WIDGET)
        && !stateManager.hasStoryNpcToken()) {
      trackerState = WAIT_FOR_NPC_TOKEN;
      waitingForBadge = false;
      displayScanNpcToken();
      return false;
    }

    String badgeUID, missionCardUID, selectedLocs, restoredLockedDiff;
    int restoredState = WAIT_FOR_BADGE, visitedCount = 0;
    unsigned long restoredStartTime = 0;

    stateManager.restoreSessionSnapshot(
      badgeUID, missionCardUID, restoredState, visitedCount,
      selectedLocs, restoredStartTime, restoredLockedDiff);

    if (badgeUID.length() == 0) return false;
    Serial.printf("[STATE] %lums  Restoring from snapshot: state=%d, badge=%s, mission=%s\n", millis(), restoredState, badgeUID.c_str(), missionCardUID.c_str());

    if (selectedLocs.length() > 0) stateManager.setSelectedLocations(selectedLocs);
    stateManager.setVisitedCount(visitedCount);

    // Restore player list
    for (int i = 0; i < MAX_PLAYERS; i++) playerUUIDs[i] = "";
    playerCount = 0;
    int start = 0;
    while (playerCount < MAX_PLAYERS && start < (int)badgeUID.length()) {
      int comma = badgeUID.indexOf(',', start);
      String tok = (comma == -1) ? badgeUID.substring(start) : badgeUID.substring(start, comma);
      tok.trim();
      if (tok.length() > 0) playerUUIDs[playerCount++] = tok;
      if (comma == -1) break;
      start = comma + 1;
    }

    currentBadgeUID       = (playerCount > 0) ? playerUUIDs[0] : "";
    currentMissionCardUID = missionCardUID;
    trackerState          = (TrackerState)restoredState;
    missionStartTime      = restoredStartTime;
    lastTimerUpdate       = 0;
    lockedDifficulty      = restoredLockedDiff;
    stateManager.setPlayerUUID(badgeUID);

    if (lockedDifficulty.length() > 0) {
      missionLocked = true; awaitingCompletionScan = true;
    } else {
      missionLocked = false; awaitingCompletionScan = false;
    }
    completionPromptShown = false;

    if (trackerState == WAIT_FOR_BADGE)          { waitingForBadge = true; displayScanBadge(); return true; }
    if (trackerState == WAIT_FOR_MISSION_CARD)   {
      waitingForBadge = false;
      if (currentMode == MODE_STORY_MISSION_WIDGET) displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
      else startFreeRoamFlow(false);
      return true;
    }
    if (trackerState == RUN_MISSION) {
      waitingForBadge = false;
      if (currentMission) { delete currentMission; currentMission = nullptr; }
      currentMission = new FreeRoamMission(stateManager);
      currentMission->setup();
      currentMission->setMissionStartMs(missionStartTime);
      currentMission->updateDisplay();
      return true;
    }

    trackerState = WAIT_FOR_BADGE; waitingForBadge = true; displayScanBadge();
    return true;
  }

  // ============================================================
  // processCardScan() Ã¢â‚¬â€ main RFID dispatch
  // ============================================================
  void processCardScan(String cardUID) {
    static String lastScannedTag = "";
    if (cardUID != lastScannedTag) {
      stateManager.saveSessionSnapshot(
        currentBadgeUID, currentMissionCardUID, trackerState,
        currentMission ? currentMission->getVisitCount() : 0,
        currentMission ? currentMission->getSelectedLocationsString() : "",
        missionStartTime, lockedDifficulty);
      lastScannedTag = cardUID;
    }
    Serial.printf("[RFID] %lums  Card scanned: %s  (state: %s)\n", millis(), cardUID.c_str(), stateName(trackerState));
    logger.log("Card: " + cardUID);

    // Ã¢â€â‚¬Ã¢â€â‚¬ 1. GATEKEEPER: bypass & reset tags Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    for (const auto& tag : FULL_RESET_TAGS) {
      if (cardUID == tag) { Serial.printf("[ACTION] %lums  FULL RESET triggered by tag\n", millis()); logger.log("FULL RESET"); fullReset(); return; }
    }

    for (int i = 0; i < NUM_ADMIN_BADGE_TAGS; i++) {
      if (cardUID == ADMIN_BADGE_TAGS[i]) {
        if (trackerState == ADMIN_MODE) {
          Serial.printf("[ACTION] %lums  Admin badge Ã¢â‚¬â€ exiting admin\n", millis());
          exitAdminMode();
        } else {
          Serial.printf("[ACTION] %lums  Admin badge Ã¢â‚¬â€ entering admin mode\n", millis());
          trackerState = ADMIN_MODE;
          adminInMenu = true; adminMenuSelection = 0;
          displayAdminMode();
          logger.log("Admin: enter");
        }
        return;
      }
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ WiFi Config scan Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (trackerState == WIFI_CONFIG) {
      for (int i = 0; i < NUM_WIFI_PRESETS; i++) {
        if (cardUID == WIFI_PRESETS[i].tagUID) {
          stateManager.setWiFiCredentials(WIFI_PRESETS[i].ssid, WIFI_PRESETS[i].password);
          displayMessage(DisplayText::WIFI_SAVED, COLOR_SUCCESS, 2000);
          trackerState = ADMIN_MODE; adminInMenu = true; adminMenuSelection = 0;
          displayAdminMode();
          return;
        }
      }
      displayError("WiFi Preset\\nNot Found");
      delay(2000);
      displayMultiLineMessage(DisplayText::WIFI_CONFIG_MODE, DisplayText::WIFI_SCAN_SSID, COLOR_PROCESSING);
      return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ Admin Mode card scan Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (trackerState == ADMIN_MODE) {
      if (!adminInMenu && adminMenuSelection == 6) {
        String tagInfo = identifyTag(cardUID);
        logger.log("Scan: " + tagInfo.substring(0, 30));
        tagInfo.startsWith("UNKNOWN TAG") ? playErrorTone() : playAdminAcceptTone();
        displayTagInfo(tagInfo);
      }
      return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ Relay mode Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    // First-run dial assignment. Store the NPC-linked Loot/Item UUID in NVS.
    if (trackerState == WAIT_FOR_NPC_TOKEN) {
      String token = normalizeRfidToken(cardUID);
      if (token.length() == 0) {
        displayError("INVALID NPC TAG");
        delay(1500);
        displayScanNpcToken();
        return;
      }
      storyNpcToken = token;
      stateManager.setStoryNpcToken(storyNpcToken);
      stateManager.clearSavedTrackerState();
      stateManager.clearSavedMissionStart();
      Serial.printf("[NPC] %lums  Assigned story NPC token: %s\n", millis(), storyNpcToken.c_str());
      logger.log("NPC token assigned: " + storyNpcToken);
      playAdminAcceptTone();
      displayMultiLineMessage("NPC TAG SAVED", storyNpcToken, COLOR_SUCCESS);
      delay(1200);
      trackerState = WAIT_FOR_BADGE;
      waitingForBadge = true;
      displayScanBadge();
      return;
    }

    if (trackerState == RELAY_WAIT_BADGE) {
      relayBadgeUID = cardUID;
      Serial.printf("[ACTION] %lums  Relay badge scanned: %s\n", millis(), cardUID.c_str());
      logger.log("Relay: " + cardUID);
      int httpCode = sendRelayUpdate(cardUID);
      relayLastSuccess = (httpCode == 200 || httpCode == 201);
      relayLastResponse = String(httpCode);
      Serial.printf("[ACTION] %lums  Relay result: HTTP %d (%s)\n", millis(), httpCode, relayLastSuccess ? "OK" : "FAILED");
      displayRelayBadgePrompt();
      return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ 2. STANDARD CONTROL TAGS Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    for (int i = 0; i < NUM_RESET_TAGS; i++) {
      if (cardUID == RESET_TAG[i]) { reset(); return; }
    }

    bool isScrubUID = false;
    for (int i = 0; i < NUM_SCRUB_MISSION_TAGS; i++) {
      if (!SCRUB_MISSION_TAGS[i].isEmpty() && cardUID == SCRUB_MISSION_TAGS[i]) { isScrubUID = true; break; }
    }
    for (int i = 0; i < NUM_BADGE_RESET_TAGS; i++) {
      if (cardUID == BADGE_RESET_TAGS[i]) {
        if (isScrubUID && currentMission) break;
        resetBadgeForReuse();
        return;
      }
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ A. WAITING FOR BADGE Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (trackerState == WAIT_FOR_BADGE) {
      // Remove badge if already registered
      int foundIdx = -1;
      for (int i = 0; i < playerCount; i++) { if (playerUUIDs[i] == cardUID) { foundIdx = i; break; } }
      if (foundIdx >= 0) {
        for (int i = foundIdx; i < playerCount - 1; i++) playerUUIDs[i] = playerUUIDs[i + 1];
        playerUUIDs[--playerCount] = "";
        Serial.printf("[ACTION] %lums  Player removed: %s (remaining: %d)\n", millis(), cardUID.c_str(), playerCount - 1);
        logger.log("Player removed: " + cardUID);
        playRemoveTone();
        displayMessage("Player Removed", COLOR_WARNING, 500);
        persistPlayerList();
        currentBadgeUID = (playerCount > 0) ? playerUUIDs[0] : "";
        (playerCount > 0) ? displayPlayerRegistration() : displayScanBadge();
        return;
      }

      if (playerCount >= MAX_PLAYERS) {
        displayMessage("Max Players", COLOR_WARNING, 800);
        displayPlayerRegistration();
        return;
      }

      // Scrubbed badge check
      if (scrubbedBadgeUIDs.count(cardUID)) {
        unsigned long secsLeft = getBadgeCooldownSecondsRemaining(cardUID);
        if (secsLeft > 0) {
          char buf[32]; snprintf(buf, sizeof(buf), "SCRUBBED \n%lu:%02lu left", secsLeft / 60, secsLeft % 60);
          playSound(SND_BADGE_COOLDOWN, SND_BADGE_COOLDOWN_LEN, 255, "BADGE_COOLDOWN", EXT_BADGE_COOLDOWN);
          displayError(buf); delay(1500); displayScanBadge();
          return;
        }
        scrubbedBadgeUIDs.erase(cardUID);
      }

      // Cooldown check
      if (isBadgeAlreadyCompleted(cardUID)) {
        unsigned long secsLeft = getBadgeCooldownSecondsRemaining(cardUID);
        char buf[32]; snprintf(buf, sizeof(buf), "Cooldown:\n%lu:%02lu left", secsLeft / 60, secsLeft % 60);
        playSound(SND_BADGE_COOLDOWN, SND_BADGE_COOLDOWN_LEN, 255, "BADGE_COOLDOWN", EXT_BADGE_COOLDOWN);
        displayError(buf); delay(1500); displayScanBadge();
        return;
      }

      // Register player
      playerUUIDs[playerCount++] = cardUID;
      Serial.printf("[ACTION] %lums  Player %d registered: %s\n", millis(), playerCount, cardUID.c_str());
      logger.log("Player " + String(playerCount) + ": " + cardUID);
      displayMessage("Player Added", COLOR_SUCCESS, 1500);
      displayPlayerRegistration();
      currentBadgeUID = playerUUIDs[0];
      persistPlayerList();

      // Badge-reset restore
      if (awaitingBadgeRestore) {
        awaitingBadgeRestore = false;
        int saved = stateManager.getSavedTrackerState();
        unsigned long savedMs = stateManager.getSavedMissionStart();
        stateManager.clearSavedMissionStart();
        stateManager.clearSavedTrackerState();
        if (saved >= 0) savedTrackerState = saved;

        if (savedTrackerState == WAIT_FOR_MISSION_CARD) {
          if (currentMode == MODE_STORY_MISSION_WIDGET) {
            trackerState = WAIT_FOR_MISSION_CARD;
            displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
          } else {
            startFreeRoamFlow(false);
          }
        } else if (savedTrackerState == RUN_MISSION) {
          trackerState = RUN_MISSION;
          displayMessage(DisplayText::RESUMING_MISSION, COLOR_INFO, 1000);
          if (currentMission) {
            if (savedMs > 0) {
              missionStartTime = savedMs; lastTimerUpdate = millis();
              currentMission->setMissionStartMs(missionStartTime);
              String ld = stateManager.getLockedDifficulty();
              if (ld.length() > 0) { missionLocked = true; awaitingCompletionScan = true; lockedDifficulty = ld; }
            }
            currentMission->updateDisplay();
          } else {
            startFreeRoamFlow(false);
          }
        } else {
          trackerState = WAIT_FOR_BADGE;
          displayPlayerRegistration();
        }
        savedTrackerState = -1; savedMissionCardUID = "";
      } else {
        trackerState = WAIT_FOR_BADGE; waitingForBadge = true; displayPlayerRegistration();
      }
      return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ B. WAITING FOR MISSION CARD (legacy state) Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (trackerState == WAIT_FOR_MISSION_CARD) {
      if (currentMode == MODE_STORY_MISSION_WIDGET) {
        startStoryRoundFlow(true, cardUID);
      } else {
        startFreeRoamFlow(true);
      }
      return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ C0. SCRUB MISSION TAG Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    {
      bool isScrub = false;
      for (int i = 0; i < NUM_SCRUB_MISSION_TAGS; i++) {
        if (!SCRUB_MISSION_TAGS[i].isEmpty() && cardUID == SCRUB_MISSION_TAGS[i]) { isScrub = true; break; }
      }
      if (isScrub && currentMission) {
        Serial.printf("[ACTION] %lums  Mission SCRUBBED\n", millis());
        logger.log("Mission scrubbed");
        playSound(SND_MISSION_SCRUBBED, SND_MISSION_SCRUBBED_LEN, 255, "MISSION_SCRUBBED", EXT_MISSION_SCRUBBED);
        markBadgesAsCompleted();
        for (int i = 0; i < playerCount; i++) {
          if (playerUUIDs[i].length() > 0) scrubbedBadgeUIDs.insert(playerUUIDs[i]);
        }
        lastCompletedMissionCardUID = currentMissionCardUID;
        currentBadgeUID = ""; currentMissionCardUID = "";
        resetBadgeOnly();
        delay(2000);
        return;
      }
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ C. COMPLETION TAG Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (currentMission && std::find(std::begin(COMPLETE_TAGS), std::end(COMPLETE_TAGS), cardUID) != std::end(COMPLETE_TAGS)) {
      if (missionLocked && awaitingCompletionScan) {
        Serial.printf("[ACTION] %lums  Completion tag scanned (locked, difficulty: %s)\n", millis(), lockedDifficulty.c_str());
        if (lockedDifficulty.isEmpty() || lockedDifficulty == "None") {
          Serial.printf("[ACTION] %lums  No difficulty detected Ã¢â‚¬â€ resetting\n", millis());
          displayMessage("No Difficulty\\nDetected", COLOR_WARNING, 2500);
          currentBadgeUID = ""; currentMissionCardUID = "";
          resetBadgeOnly();
        } else {
          if (sendCombinedCompletionRequest(lockedDifficulty)) {
            markBadgesAsCompleted();
            lastCompletedMissionCardUID = currentMissionCardUID;
            currentBadgeUID = ""; currentMissionCardUID = "";
            resetBadgeOnly();
            delay(2000);
          } else { completionPromptShown = false; }
        }
        return;
      }
      if (!currentMission->isComplete()) {
        Serial.printf("[ACTION] %lums  Completion tag scanned but locations not all visited\n", millis());
        displayError(DisplayText::VISIT_LOCATIONS_FIRST); return;
      }
      String diff = currentMission->getCurrentDifficulty();
      Serial.printf("[ACTION] %lums  Completion tag scanned Ã¢â‚¬â€ sending results (difficulty: %s)\n", millis(), diff.c_str());
      if (sendCombinedCompletionRequest(diff)) {
        markBadgesAsCompleted();
        lastCompletedMissionCardUID = currentMissionCardUID;
        currentBadgeUID = ""; currentMissionCardUID = "";
        resetBadgeOnly(); delay(2000);
      } else { if (currentMission) currentMission->updateDisplay(); }
      return;
    }

    // Completion tag with no active mission
    if (std::find(std::begin(COMPLETE_TAGS), std::end(COMPLETE_TAGS), cardUID) != std::end(COMPLETE_TAGS)) {
      Serial.printf("[ACTION] %lums  Completion tag scanned but no active mission\n", millis());
      displayError("No Active\\nMission"); return;
    }

    // Ã¢â€â‚¬Ã¢â€â‚¬ D. LOCATION SCANNING Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
    if (trackerState == RUN_MISSION && currentMission) {
      int locIdx = -1, resType = -1;
      Serial.printf("[ACTION] %lums  POI tag scanned\n", millis());
      currentMission->processLocation(cardUID);
    }
  }

  // ============================================================
  // handleButtonPress()
  // ============================================================
  void handleButtonPress() {
    Serial.printf("[ACTION] %lums  Button pressed (state: %s)\n", millis(), stateName(trackerState));

    // Admin mode button
    if (trackerState == ADMIN_MODE) {
      if (!adminInMenu) {
        if (adminMenuSelection == 6) { adminInMenu = true; logger.log("Admin: exit tag scan"); displayAdminMode(); }
      } else {
        executeAdminMenuItem();
      }
      return;
    }

    // Timer overlay toggle
    if (trackerState == RUN_MISSION && currentMission && !missionLocked) {
      showingTimerOverlay = !showingTimerOverlay;
      if (showingTimerOverlay) timerOverlayStartTime = millis();
    }
    // Confirm players
    else if (trackerState == CONFIRM_PLAYERS) {
      applyConfirmPlayersSelection();
    }
    // Badge-wait button Ã¢â€ â€™ confirm
    else if (trackerState == WAIT_FOR_BADGE && playerCount > 0) {
      confirmSelection = 0; confirmEncoderPos = M5Dial.Encoder.read();
      trackerState = CONFIRM_PLAYERS; displayConfirmPlayers();
    }
    // Relay Ã¢â€ â€™ switch to mission
    else if (trackerState == RELAY_WAIT_BADGE) {
      Serial.printf("[ACTION] %lums  Switching from Relay to Mission mode\n", millis());
      currentMode = MODE_MISSION_WIDGET; stateManager.setOperationalMode(currentMode);
      displayMessage("Switched to\\nMission Mode", COLOR_SUCCESS, 1500);
      shutdownWiFi();
      if (stateManager.hasStoryNpcToken()) {
        trackerState = WAIT_FOR_BADGE;
        displayScanBadge();
      } else {
        trackerState = WAIT_FOR_NPC_TOKEN;
        displayScanNpcToken();
      }
    }
  }

  // ============================================================
  // update() Ã¢â‚¬â€ called every loop iteration
  // ============================================================
  void update() {
    if (currentMode == MODE_RELAY && WiFi.status() != WL_CONNECTED && millis() - lastWiFiReconnectAttemptMs >= WIFI_BACKGROUND_RECONNECT_INTERVAL_MS) {
      lastWiFiReconnectAttemptMs = millis();
      Serial.printf("[WIFI] %lums  Background reconnect attempt\n", millis());
      logger.log("WiFi: background reconnect");
      WiFi.reconnect();
    }

    // Screen timeout
    if (screenOn && (millis() - lastActivityMs > SCREEN_TIMEOUT_MS)) {
      screenOn = false; M5Dial.Display.setBrightness(0);
    }

    // Confirm-players encoder
    if (trackerState == CONFIRM_PLAYERS) {
      long newPos = M5Dial.Encoder.read();
      long delta = newPos - confirmEncoderPos;
      if (abs(delta) >= 4) {
        int newSel = (delta > 0) ? 1 : 0;
        if (newSel != confirmSelection) { confirmSelection = newSel; confirmEncoderPos = newPos; displayConfirmPlayers(); }
        else confirmEncoderPos = newPos;
      }
      return;
    }

    // Relay encoder exit
    if (trackerState == RELAY_WAIT_BADGE) {
      static int relEnc = M5Dial.Encoder.read();
      if (M5Dial.Encoder.read() - relEnc <= -4) {
        currentMode = MODE_MISSION_WIDGET; stateManager.setOperationalMode(currentMode);
        displayMessage("Switched to\\nMission Mode", COLOR_SUCCESS, 1500);
        shutdownWiFi();
        if (stateManager.hasStoryNpcToken()) {
          trackerState = WAIT_FOR_BADGE;
          displayScanBadge();
        } else {
          trackerState = WAIT_FOR_NPC_TOKEN;
          displayScanNpcToken();
        }
        relEnc = M5Dial.Encoder.read();
        return;
      }
    }

    // Admin encoder
    if (trackerState == ADMIN_MODE) {
      int encVal = M5Dial.Encoder.read();
      int delta = encVal - lastEncoderValue;
      if (abs(delta) >= 4) {
        int steps = delta / 4;
        if (adminInMenu) {
          int dir = (steps > 0) ? 1 : -1;
          for (int i = 0; i < abs(steps); i++) adminMenuSelection = (adminMenuSelection + dir + ADMIN_MENU_ITEMS) % ADMIN_MENU_ITEMS;
          displayAdminMode();
        }
        lastEncoderValue += steps * 4;
      }
      return;
    }

    // Mission display & timer
    if (currentMission) {
      if (!missionLocked) currentMission->updateDisplay();

      if (currentMission->isComplete() && !missionLocked) {
        if (millis() % 10000 < 1000) {
          displayMultiLineMessage(DisplayText::TAP_TO_COMPLETE, DisplayText::rewardLine(currentMission->getCurrentDifficulty()), COLOR_ERROR);
          delay(1000);
        }
      }

      // Timer management
      if (trackerState == RUN_MISSION && !missionLocked && missionStartTime > 0) {
        unsigned long now = millis();
        if (now - lastTimerUpdate >= 1000) {
          lastTimerUpdate = now;
          unsigned long elapsed = now - missionStartTime;
          if (elapsed >= MISSION_TIMEOUT_MS) {
            Serial.printf("[ACTION] %lums  Mission timer expired\n", millis());
            String diff = currentMission->getCurrentDifficulty();
            if (diff.isEmpty() || diff == "None") {
              markBadgesAsCompleted();
              lastCompletedMissionCardUID = currentMissionCardUID;
              missionLocked = true; awaitingCompletionScan = true;
              lockedDifficulty = "";
              stateManager.clearMissionStartTime(); stateManager.clearLockedDifficulty();
              completionPromptShown = false;
              playSound(SND_TIMEOUT, SND_TIMEOUT_LEN, 255, "TIMEOUT", EXT_TIMEOUT);
              drawTimerExpiredAnimation();
              displayMultiLineMessage(DisplayText::TIMEOUT_TITLE, DisplayText::TIMEOUT_ACTION, COLOR_WARNING);
            } else {
              missionLocked = true; awaitingCompletionScan = true;
              lockedDifficulty = diff;
              stateManager.setLockedDifficulty(lockedDifficulty);
              completionPromptShown = false;
              playSound(SND_TIMEOUT, SND_TIMEOUT_LEN, 255, "TIMEOUT", EXT_TIMEOUT);
              drawTimerExpiredAnimation();
              displayMultiLineMessage(DisplayText::COMPLETION_TITLE, DisplayText::COMPLETION_SUBTITLE, COLOR_INFO);
              displayMultiLineMessage(DisplayText::REWARD_LABEL, lockedDifficulty, COLOR_INFO);
            }
          }
        }

        // Timer overlay
        if (showingTimerOverlay) {
          unsigned long now2 = millis();
          if (now2 - timerOverlayStartTime >= 5000) {
            showingTimerOverlay = false;
            currentMission->updateDisplay();
          } else {
            drawTimerOverlay(now2);
          }
        }
      }

      // Locked prompt
      if (trackerState == RUN_MISSION && missionLocked && !completionPromptShown) {
        if (lockedDifficulty.isEmpty() || lockedDifficulty == "None") {
          displayMultiLineMessage(DisplayText::TIMEOUT_TITLE, DisplayText::TIMEOUT_ACTION, COLOR_WARNING);
        } else {
          displayMultiLineMessage(DisplayText::COMPLETION_TITLE, DisplayText::COMPLETION_SUBTITLE, COLOR_INFO);
          displayMultiLineMessage(DisplayText::REWARD_LABEL, lockedDifficulty, COLOR_INFO);
        }
        completionPromptShown = true;
      }
    }
  }

  // ============================================================
  // Admin menu display (delegates to AdminMenu where possible)
  // ============================================================
  void displayAdminMode() {
    flashScreen(TFT_BLACK);
    drawStatusIcon(ICON_INFO);
    drawAlertBorder(TFT_DARKGREY);
    int cx = M5Dial.Display.width() / 2;
    int cy = M5Dial.Display.height() / 2;
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString("SELECT MODE", cx, 20);

    struct MI { const char* icon; uint16_t bg; const char* l1; const char* l2; const char* desc; uint16_t labelCol; };
    MI items[] = {
      {"i", TFT_NAVY,      "DEVICE","INFO",   "Serial & MAC address", TFT_CYAN},
      {"M", TFT_DARKGREEN, "MISSION","WIDGET", "Story missions & badges", TFT_GREEN},
      {"S", TFT_DARKGREEN, "STORY","ROUND", "Shared NPC round mission", TFT_GREEN},
      {"R", TFT_MAROON,    "RELAY","MODE",     "Badge relay updates", TFT_ORANGE},
      {"D", devMode ? TFT_DARKGREEN : TFT_MAROON, "DEV MODE", devMode ? "ON" : "OFF", "GURU_HOME only", devMode ? TFT_GREEN : TFT_RED},
      {"L", 0x2104,        "VIEW","LOG",       "Mission event history", TFT_YELLOW},
      {"?", TFT_DARKCYAN,  "SCAN TAG","INFO",  "Identify any card", TFT_CYAN},
      {"N", TFT_DARKGREEN, "NPC TAG","RESET",  "Clear saved NPC assignment", TFT_GREEN},
      {"X", TFT_BLACK,     "EXIT","ADMIN",     "Return to Main", TFT_WHITE},
    };

    if (adminMenuSelection == 8) {
      // Exit icon
      M5Dial.Lcd.drawRect(cx - 20, cy - 35, 40, 50, TFT_RED);
      M5Dial.Lcd.drawLine(cx - 20, cy - 10, cx + 20, cy - 10, TFT_RED);
      M5Dial.Lcd.fillTriangle(cx + 10, cy - 30, cx + 25, cy - 15, cx + 10, cy, TFT_ORANGE);
      M5Dial.Lcd.drawLine(cx - 5, cy - 15, cx + 10, cy - 15, TFT_ORANGE);
      M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_WHITE);
      M5Dial.Display.drawString("EXIT ADMIN", cx, cy + 30);
      M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_DARKGREY);
      M5Dial.Display.drawString("Return to Main", cx, cy + 55);
    } else {
      MI& m = items[adminMenuSelection];
      M5Dial.Lcd.fillCircle(cx, cy - 30, 35, m.bg);
      M5Dial.Display.setTextColor(TFT_WHITE); M5Dial.Display.setTextSize(3);
      M5Dial.Display.drawString(m.icon, cx, cy - 35);
      M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(m.labelCol);
      M5Dial.Display.drawString(m.l1, cx, cy + 20);
      if (strlen(m.l2) > 0) M5Dial.Display.drawString(m.l2, cx, cy + 40);
      M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_DARKGREY);
      M5Dial.Display.drawString(m.desc, cx, cy + 65);
    }

    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_DARKGREY);
    M5Dial.Display.drawString("Rotate: Navigate", cx, 200);
    M5Dial.Display.drawString("Press: Select", cx, 214);
    M5Dial.Display.setTextColor(TFT_YELLOW);
    M5Dial.Display.drawString(DisplayText::ADMIN_EXIT, cx, 232);
  }

  void exitAdminMode() {
    const char* modeLabel = currentMode == MODE_RELAY ? "Relay" : (currentMode == MODE_STORY_MISSION_WIDGET ? "Story Round" : "Mission");
    Serial.printf("[ACTION] %lums  Exiting admin mode (mode: %s)\n", millis(), modeLabel);
    adminInMenu = true; adminMenuSelection = 0;
    logger.log("Admin: exit");
    if (currentMode == MODE_MISSION_WIDGET || currentMode == MODE_STORY_MISSION_WIDGET) {
      if (stateManager.hasStoryNpcToken()) {
        trackerState = WAIT_FOR_BADGE;
        displayScanBadge();
      } else {
        trackerState = WAIT_FOR_NPC_TOKEN;
        displayScanNpcToken();
      }
    } else {
      trackerState = RELAY_WAIT_BADGE;
      displayRelayBadgePrompt();
    }
  }

  // ============================================================
  // Display methods
  // ============================================================
  void displaySplashScreen() {
    M5Dial.Display.fillScreen(TFT_BLACK);
    M5Dial.Display.drawJpg(guru_logo_jpg, guru_logo_jpg_len, 0, 0, 240, 240);
    // Play boot sound (001.mp3) via AudioPlayer Ã¢â‚¬â€ non-blocking, tracks start time
    if (audioPlayerReady) {
      audioPlayer.playAudioByName("001.mp3");
      _splashAudioStartMs = millis();
      Serial.println("[AUDIO] Playing boot sound 001.mp3");
    } else {
      _splashAudioStartMs = millis();
      Serial.println("[AUDIO] AudioPlayer not ready Ã¢â‚¬â€ skipping boot sound");
    }
    // No blocking delay here Ã¢â‚¬â€ begin() will hold on splash until audio is done
  }

  void displayMessage(String message, uint16_t color, int duration = 0) {
    if (color == COLOR_SUCCESS || color == TFT_GREEN) playSuccessTone();
    else if (color == COLOR_ERROR || color == TFT_RED) playErrorTone();
    else if (color == COLOR_INFO || color == TFT_BLUE) playInfoTone();

    flashScreen(color);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_WHITE);

    if (color == COLOR_SUCCESS || color == TFT_GREEN)      { drawStatusIcon(ICON_SUCCESS); drawAlertBorder(TFT_WHITE); }
    else if (color == COLOR_ERROR || color == TFT_RED)     { drawStatusIcon(ICON_ERROR);   drawAlertBorder(TFT_WHITE); }
    else if (color == COLOR_WARNING || color == TFT_ORANGE){ drawStatusIcon(ICON_WARNING); drawAlertBorder(TFT_WHITE); }
    else                                                   { drawStatusIcon(ICON_INFO);    drawAlertBorder(TFT_DARKGREY); }

    const int MAX_CHARS = 15, LINE_H = 20;
    std::vector<String> lines;
    String cur;
    for (int i = 0; i < (int)message.length(); ) {
      if (message[i] == '\\' && i + 1 < (int)message.length() && message[i+1] == 'n') {
        if (cur.length() > 0) lines.push_back(cur);
        cur = ""; i += 2; continue;
      }
      cur += message[i];
      if ((int)cur.length() >= MAX_CHARS) {
        int sp = cur.lastIndexOf(' ');
        if (sp > 0) { lines.push_back(cur.substring(0, sp)); cur = cur.substring(sp + 1); }
        else { lines.push_back(cur); cur = ""; }
      }
      i++;
    }
    if (cur.length() > 0) lines.push_back(cur);

    int totalH = lines.size() * LINE_H;
    int startY = (M5Dial.Display.height() - totalH) / 2 + LINE_H / 2;
    for (int j = 0; j < (int)lines.size(); j++)
      M5Dial.Display.drawString(lines[j], M5Dial.Display.width() / 2, startY + j * LINE_H);

    if (duration > 0) delay(duration);
  }

  void displayError(String msg) {
    playErrorTone();
    displayMessage(msg, COLOR_ERROR, 0);
    if (LOUD_MODE) { delay(80); playErrorTone(); }
  }

  void displayMultiLineMessage(String l1, String l2, uint16_t color) {
    M5Dial.Lcd.fillScreen(color);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString(l1, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 10);
    M5Dial.Display.drawString(l2, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 10);
  }

  void displayScanNpcToken() {
    flashScreen(TFT_BLACK);
    drawStatusIcon(ICON_INFO);
    drawAlertBorder(TFT_CYAN);
    drawBadgeIcon(120, 50, 70);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString("ASSIGN DIAL", 120, 135);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("SCAN NPC TAG", 120, 165);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(TFT_LIGHTGREY);
    M5Dial.Display.drawString("Saved until cleared in Admin", 120, 195);
  }

  void displayScanBadge() {
    if (playerCount > 0) { displayPlayerRegistration(); return; }
    flashScreen(TFT_BLACK); drawStatusIcon(ICON_INFO); drawAlertBorder(TFT_DARKGREY);
    drawBadgeIcon(120, 50, 70);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("Hover Badge", 120, 180);
  }

  void displayScanMissionCard() {
    flashScreen(TFT_BLACK); drawStatusIcon(ICON_INFO); drawAlertBorder(TFT_DARKGREY);
    drawGameCaseIcon(70, 30, 100, 130);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("Scan Mission", 120, 190);
  }

  void displayPlayerRegistration() {
    flashScreen(TFT_BLACK); drawStatusIcon(ICON_INFO); drawAlertBorder(TFT_DARKGREY);
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("PLAYERS", 120, 25);
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString(String(playerCount) + "/" + String(MAX_PLAYERS), 120, 45);

    int total = std::max(playerCount, 1);
    int rows = (total <= 5) ? 1 : 2;
    int cols = (rows == 1) ? total : (total + 1) / 2;
    int iconSize = (cols <= 3) ? 22 : (cols <= 4) ? 18 : 14;
    int spacing = 200 / (cols + 1);
    int rowSpacing = iconSize + iconSize * 1.2 + 14;
    int baseY = (rows == 1) ? 100 : 80;
    for (int i = 0; i < total; i++) {
      int row = (rows == 1) ? 0 : i / cols;
      int col = (rows == 1) ? i : i % cols;
      int rc = (rows == 1) ? cols : ((row == 0) ? cols : total - cols);
      int rw = (rc - 1) * spacing;
      int x = 120 - rw / 2 + col * spacing;
      int y = baseY + row * rowSpacing;
      drawPlayerIcon(x, y, iconSize, i < playerCount);
    }
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_WHITE);
    if (playerCount >= MAX_PLAYERS) { M5Dial.Display.drawString("All Players Ready", 120, 200); M5Dial.Display.drawString("Press Button", 120, 215); }
    else if (playerCount > 0)       M5Dial.Display.drawString("Scan More or Press Button", 120, 200);
    else                            M5Dial.Display.drawString("Scan Player Badge", 120, 200);
  }

  void displayConfirmPlayers() {
    M5Dial.Lcd.fillScreen(TFT_BLACK);
    int cx = M5Dial.Display.width() / 2;
    M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("DONE ADDING", cx, 40);
    M5Dial.Display.drawString("PLAYERS?", cx, 58);
    M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString(String(playerCount) + " player" + (playerCount == 1 ? "" : "s") + " registered", cx, 80);
    // YES
    if (confirmSelection == 0) { M5Dial.Display.fillRoundRect(cx-55,105,110,36,8,TFT_GREEN); M5Dial.Display.setTextColor(TFT_BLACK); }
    else { M5Dial.Display.drawRoundRect(cx-55,105,110,36,8,TFT_DARKGREY); M5Dial.Display.setTextColor(TFT_DARKGREY); }
    M5Dial.Display.setTextSize(2); M5Dial.Display.drawString("YES", cx, 123);
    // NO
    if (confirmSelection == 1) { M5Dial.Display.fillRoundRect(cx-55,153,110,36,8,TFT_RED); M5Dial.Display.setTextColor(TFT_WHITE); }
    else { M5Dial.Display.drawRoundRect(cx-55,153,110,36,8,TFT_DARKGREY); M5Dial.Display.setTextColor(TFT_DARKGREY); }
    M5Dial.Display.drawString("NO", cx, 171);
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("Scroll to select", cx, 210);
    M5Dial.Display.drawString("Click to confirm", cx, 225);
  }

  void displayRelayBadgePrompt() {
    M5Dial.Display.fillScreen(TFT_BLACK);
    drawAlertBorder(TFT_DARKGREY);
    int cx = M5Dial.Display.width() / 2;
    M5Dial.Display.setTextDatum(MC_DATUM);

    // Title
    M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString("RELAY MODE", cx, 30);

    // Serial number
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_DARKGREY);
    String sn = DEVICE_SERIAL_NUM;
    if (sn.length() > 20) sn = sn.substring(0, 18) + "..";
    M5Dial.Display.drawString(sn, cx, 55);

    // Instructions
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("Scan badge", cx, 75);
    M5Dial.Display.drawString("to load UUID", cx, 95);

    // Show last result if available
    if (relayBadgeUID.length() > 0) {
      M5Dial.Display.setTextColor(TFT_CYAN);
      M5Dial.Display.drawString("Last UUID:", cx, 130);
      M5Dial.Display.setTextColor(TFT_WHITE);
      String shortUuid = relayBadgeUID;
      shortUuid.replace(" ", "");
      if (shortUuid.length() > 12) shortUuid = shortUuid.substring(0, 12) + "...";
      M5Dial.Display.drawString(shortUuid, cx, 150);

      uint16_t respColor = relayLastSuccess ? COLOR_SUCCESS : COLOR_ERROR;
      M5Dial.Display.setTextColor(respColor);
      M5Dial.Display.drawString(relayLastResponse, cx, 180);
    }

    // Hint
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString("Press button to exit", cx, 220);
  }

  void displayLogFile() {
    String logContent = logger.readLogTail();
    flashScreen(TFT_BLACK); drawAlertBorder(TFT_DARKGREY);
    const int cx = 120, lineH = 10, maxChars = 26, yStart = 30, yEnd = 218;
    M5Dial.Display.setTextDatum(MC_DATUM); M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(TFT_CYAN); M5Dial.Display.drawString(DisplayText::LOG_TITLE, cx, 14);
    M5Dial.Display.drawLine(40, 22, 200, 22, TFT_DARKGREY);
    M5Dial.Display.setTextColor(TFT_WHITE);
    int y = yStart, ci = 0;
    while (ci < (int)logContent.length() && y <= yEnd) {
      int nl = logContent.indexOf('\n', ci); if (nl == -1) nl = logContent.length();
      String line = logContent.substring(ci, nl); line.trim();
      while (line.length() > 0 && y <= yEnd) {
        String chunk;
        if ((int)line.length() > maxChars) {
          int bp = maxChars;
          for (int i = maxChars - 1; i > 0; i--) { if (line[i] == ' ') { bp = i; break; } }
          chunk = line.substring(0, bp); line = line.substring(bp); line.trim();
        } else { chunk = line; line = ""; }
        M5Dial.Display.drawString(chunk, cx, y); y += lineH;
      }
      ci = nl + 1;
    }
    M5Dial.Display.drawLine(40, 222, 200, 222, TFT_DARKGREY);
    M5Dial.Display.setTextColor(TFT_YELLOW);
    M5Dial.Display.drawString(DisplayText::LOG_FOOTER, cx, 230);
    unsigned long st = millis();
    while (millis() - st < 30000UL) { M5Dial.update(); if (M5Dial.BtnA.wasPressed()) break; delay(100); yield(); }
    displayScanBadge();
  }

  void displayDeviceInfo() {
    int cx = M5Dial.Display.width()/2;
    M5Dial.Display.fillScreen(BLACK);
    M5Dial.Display.setTextColor(TFT_CYAN); M5Dial.Display.setTextSize(2); M5Dial.Display.setTextDatum(top_center);
    M5Dial.Display.drawString("DEVICE", cx, 10); M5Dial.Display.drawString("INFO", cx, 30);
    M5Dial.Display.drawLine(20,55,220,55,TFT_CYAN);
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextDatum(middle_left);
    M5Dial.Display.setTextColor(WHITE); M5Dial.Display.drawString("Serial:", 20, 70);
    M5Dial.Display.setTextColor(TFT_CYAN);
    String ser = String(DEVICE_SERIAL_NUM); if (ser.length() > 18) ser = ser.substring(0,16) + "..";
    M5Dial.Display.drawString(ser, 20, 85);
    M5Dial.Display.setTextColor(WHITE); M5Dial.Display.drawString("Mode:", 20, 105);
    M5Dial.Display.setTextColor(TFT_CYAN);
    const char* mn[] = {"Mission Widget","Relay","Story Round"};
    M5Dial.Display.drawString(mn[(int)currentMode], 90, 105);
    M5Dial.Display.setTextColor(WHITE); M5Dial.Display.drawString("Version:", 20, 125);
    M5Dial.Display.setTextColor(TFT_CYAN); M5Dial.Display.drawString(FIRMWARE_VERSION, 20, 140);
    M5Dial.Display.setTextColor(WHITE); M5Dial.Display.drawString("MAC:", 20, 160);
    M5Dial.Display.setTextColor(0x7BEF); M5Dial.Display.drawString(String(DEVICE_MAC_ADDR), 20, 175);
    M5Dial.Display.setTextColor(WHITE); M5Dial.Display.drawString("WiFi:", 20, 195);
    if (WiFi.status() == WL_CONNECTED) {
      M5Dial.Display.setTextColor(TFT_GREEN); M5Dial.Display.drawString("Connected", 70, 195);
      M5Dial.Display.setTextColor(0x7BEF); M5Dial.Display.drawString(WiFi.localIP().toString(), 20, 210);
    } else { M5Dial.Display.setTextColor(TFT_RED); M5Dial.Display.drawString("Disconnected", 70, 195); }
    M5Dial.Display.setTextColor(0x7BEF); M5Dial.Display.setTextDatum(bottom_center);
    M5Dial.Display.drawString("Press: Menu | Admin: Exit", cx, 235);
    while (true) {
      M5Dial.update();
      if (M5Dial.BtnA.wasPressed()) break;
      if (M5Dial.Rfid.PICC_IsNewCardPresent() && M5Dial.Rfid.PICC_ReadCardSerial()) {
        String uid = "";
        for (byte i = 0; i < M5Dial.Rfid.uid.size; i++) {
          uid += String(M5Dial.Rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
          uid += String(M5Dial.Rfid.uid.uidByte[i], HEX);
        }
        uid.toUpperCase();
        for (int i = 0; i < NUM_ADMIN_BADGE_TAGS; i++) { if (uid == ADMIN_BADGE_TAGS[i]) { exitAdminMode(); return; } }
      }
      delay(50);
    }
  }

  void displayTagInfo(String tagInfo) {
    M5Dial.Lcd.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM); M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_WHITE);
    int y = 70;
    String rem = tagInfo;
    while (rem.length() > 0) {
      int nl = rem.indexOf("\\n");
      String line = (nl == -1) ? rem : rem.substring(0, nl);
      rem = (nl == -1) ? "" : rem.substring(nl + 2);
      M5Dial.Display.drawString(line, M5Dial.Display.width()/2, y); y += 25;
    }
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_DARKGREY);
    M5Dial.Display.drawString("Scan next  |  Press: back", M5Dial.Display.width()/2, 220);
  }

  // ============================================================
  // Drawing helpers (icons, selection screens)
  // ============================================================
  void drawPlayerIcon(int x, int y, int size, bool filled) {
    uint16_t c = filled ? TFT_CYAN : TFT_DARKGREY;
    int hr = size / 3, bh = size * 1.2;
    if (filled) { M5Dial.Display.fillCircle(x, y, hr, c); M5Dial.Display.fillRoundRect(x-size/2, y+hr+2, size, bh, 5, c); }
    else { M5Dial.Display.drawCircle(x,y,hr,c); M5Dial.Display.drawCircle(x,y,hr-1,c); M5Dial.Display.drawRoundRect(x-size/2,y+hr+2,size,bh,5,c); M5Dial.Display.drawRoundRect(x-size/2+1,y+hr+3,size-2,bh-2,5,c); }
  }

  void drawBadgeIcon(int x, int y, int size) {
    int bw = size, bh = size * 1.3;
    M5Dial.Display.fillRoundRect(x-bw/2, y, bw, bh, 8, TFT_CYAN);
    M5Dial.Display.fillRoundRect(x-bw/2+6, y+6, bw-12, bh-12, 5, TFT_LIGHTGREY);
    int cw = bw * 0.4, cy2 = y - 5;
    M5Dial.Display.fillRect(x-cw/2, cy2, cw, 10, TFT_DARKGREY);
    M5Dial.Display.fillCircle(x, cy2+5, cw/2, TFT_DARKGREY);
    int lr = size * 0.25;
    M5Dial.Display.fillCircle(x, y+bh/2, lr, TFT_CYAN);
    M5Dial.Display.drawCircle(x, y+bh/2, lr, TFT_DARKGREY);
    int ly1 = y + bh * 0.7, lw = bw * 0.6;
    M5Dial.Display.fillRect(x-lw/2, ly1, lw, 2, TFT_DARKGREY);
    M5Dial.Display.fillRect(x-lw/2, ly1+8, lw*0.7, 2, TFT_DARKGREY);
    M5Dial.Display.fillRect(x-lw/2, ly1+16, lw*0.5, 2, TFT_DARKGREY);
  }

  void drawGameCaseIcon(int x, int y, int w, int h) {
    M5Dial.Display.fillRoundRect(x,y,w,h,8,TFT_DARKGREY);
    M5Dial.Display.fillRoundRect(x+8,y+8,w-16,h-16,5,TFT_LIGHTGREY);
    M5Dial.Display.fillRect(x+8,y+8,w-16,25,TFT_CYAN);
    int ccx = x+w/2, ccy = y+h/2+10;
    M5Dial.Display.fillCircle(ccx,ccy,30,TFT_DARKGREY);
    M5Dial.Display.fillCircle(ccx,ccy,25,TFT_SILVER);
    M5Dial.Display.fillCircle(ccx,ccy,8,TFT_DARKGREY);
    M5Dial.Display.setTextSize(1); M5Dial.Display.setTextColor(TFT_BLACK); M5Dial.Display.setTextDatum(MC_DATUM);
    M5Dial.Display.drawString("MISSION", ccx, y+8+12);
  }

  void drawMissionPassedScreen() {
    flashScreen(TFT_GREEN); drawStatusIcon(ICON_SUCCESS); drawAlertBorder(TFT_WHITE);
    int cx = M5Dial.Display.width()/2, ty = M5Dial.Display.height()/2 - 30, by = M5Dial.Display.height()/2 + 20;
    M5Dial.Display.setTextDatum(MC_DATUM);
    // Gold outlined "MISSION PASSED"
    M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_BLACK);
    for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) if (dx||dy) M5Dial.Display.drawString(DisplayText::MISSION_PASSED, cx+dx, ty+dy);
    M5Dial.Display.setTextColor(M5Dial.Display.color565(218,165,32));
    M5Dial.Display.drawString(DisplayText::MISSION_PASSED, cx, ty);
    // White outlined "+RESPECT"
    M5Dial.Display.setTextSize(3); M5Dial.Display.setTextColor(TFT_BLACK);
    for (int dx = -3; dx <= 3; dx++) for (int dy = -3; dy <= 3; dy++) if (dx||dy) M5Dial.Display.drawString(DisplayText::RESPECT_PLUS, cx+dx, by+dy);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString(DisplayText::RESPECT_PLUS, cx, by);
  }

  void drawMissionStartingScreen() {
    flashScreen(TFT_BLACK); drawStatusIcon(ICON_INFO); drawAlertBorder(TFT_WHITE);
    int cx = M5Dial.Display.width()/2, ty = M5Dial.Display.height()/2 - 24, by = M5Dial.Display.height()/2 + 18;
    M5Dial.Display.setTextDatum(MC_DATUM);

    M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_BLACK);
    for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) if (dx||dy) M5Dial.Display.drawString("MISSION", cx+dx, ty+dy);
    M5Dial.Display.setTextColor(M5Dial.Display.color565(218,165,32));
    M5Dial.Display.drawString("MISSION", cx, ty);

    M5Dial.Display.setTextSize(2); M5Dial.Display.setTextColor(TFT_BLACK);
    for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) if (dx||dy) M5Dial.Display.drawString("STARTING", cx+dx, by+dy);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("STARTING", cx, by);
  }

  // ============================================================
  // Network helpers
  // ============================================================
  void shutdownWiFi() {
    if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
      Serial.printf("[WIFI] %lums  Shutting down WiFi\n", millis());
      logger.log("WiFi: shutdown");
    }
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
  }

  bool connectToWiFi() {
    if (WiFi.status() == WL_CONNECTED) { Serial.printf("[WIFI] %lums  Already connected: %s\n", millis(), WiFi.localIP().toString().c_str()); return true; }

    // Ensure ESP-NOW/promiscuous hooks from Boss mode are not competing with WiFi HTTP usage.
    static String lastSSID = "";

    String ssid;
    String password;
    if (stateManager.hasStoredWiFiCredentials()) {
      Serial.printf("[WIFI] %lums  Connecting with stored credentials...\n", millis());
      ssid = stateManager.getWiFiSSID();
      password = stateManager.getWiFiPassword();
      logger.log("WiFi: stored");
    } else {
      Serial.printf("[WIFI] %lums  Connecting with default credentials...\n", millis());
      ssid = WIFI_SSID;
      password = WIFI_PASSWORD;
      logger.log("WiFi: default");
    }

    String hostname = String(WIFI_HOSTNAME_PREFIX) + "-" + DEVICE_SERIAL_NUM;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setHostname(hostname.c_str());
#if WIFI_STATIC_IP_ENABLED
    IPAddress localIP(WIFI_STATIC_IP_1, WIFI_STATIC_IP_2, WIFI_STATIC_IP_3, WIFI_STATIC_IP_4);
    IPAddress gateway(WIFI_GATEWAY_1, WIFI_GATEWAY_2, WIFI_GATEWAY_3, WIFI_GATEWAY_4);
    IPAddress subnet(WIFI_SUBNET_1, WIFI_SUBNET_2, WIFI_SUBNET_3, WIFI_SUBNET_4);
    IPAddress dns(WIFI_DNS_1, WIFI_DNS_2, WIFI_DNS_3, WIFI_DNS_4);
    if (!WiFi.config(localIP, gateway, subnet, dns)) {
      Serial.printf("[WIFI] %lums  Static IP config failed\n", millis());
      logger.log("WiFi: static IP config failed");
    }
#endif

    if (lastSSID != ssid || WiFi.SSID() != ssid) {
      Serial.printf("[WIFI] %lums  Begin with SSID: %s  Hostname: %s\n", millis(), ssid.c_str(), hostname.c_str());
      WiFi.begin(ssid.c_str(), password.c_str());
      lastSSID = ssid;
    } else {
      Serial.printf("[WIFI] %lums  Reusing SSID, reconnecting...\n", millis());
      WiFi.reconnect();
    }

    auto waitForWiFi = [&](const char* phase) {
      unsigned long startedAt = millis();
      unsigned long nextStatusAt = startedAt;
      while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT) {
        if (millis() >= nextStatusAt) {
          Serial.printf("[WIFI] %lums  %s waiting: status=%d elapsed=%lums\n",
                        millis(), phase, (int)WiFi.status(), millis() - startedAt);
          nextStatusAt = millis() + 1000UL;
        }
        delay(WIFI_RETRY_DELAY);
      }
      Serial.printf("[WIFI] %lums  %s finished: status=%d elapsed=%lums\n",
                    millis(), phase, (int)WiFi.status(), millis() - startedAt);
      return WiFi.status() == WL_CONNECTED;
    };

    unsigned long st = millis();
    waitForWiFi("begin");

    // One gentle reconnect attempt (no mode toggles/deinit).
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[WIFI] %lums  First attempt failed; trying reconnect...\n", millis());
      logger.log("WiFi retry: reconnect");
      WiFi.reconnect();
      st = millis();
      waitForWiFi("retry");
    }

    // Final fallback: single begin retry without forcing driver off/on.
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[WIFI] %lums  Reconnect failed; retrying begin...\n", millis());
      logger.log("WiFi retry: begin");
      WiFi.begin(ssid.c_str(), password.c_str());
      st = millis();
      waitForWiFi("retry");
    }

    if (WiFi.status() == WL_CONNECTED) {
      String connectedIp = WiFi.localIP().toString();
      Serial.printf("[WIFI] %lums  Connected OK - IP: %s (took %lums) heap=%u rssi=%d\n",
                    millis(), connectedIp.c_str(), millis() - st, (unsigned)ESP.getFreeHeap(), WiFi.RSSI());
      lastWiFiReconnectAttemptMs = millis();
      logger.log("WiFi OK: " + connectedIp);
      delay(50);
      return true;
    }
    Serial.printf("[WIFI] %lums  FAILED to connect (timeout %dms)\n", millis(), WIFI_CONNECT_TIMEOUT);
    logNetworkSnapshot("WiFi connect failed");
    return false;
  }

  int sendRelayUpdate(String uuid) {
    if (!connectToWiFi()) {
      Serial.printf("[HTTP] %lums  Relay SKIPPED Ã¢â‚¬â€ WiFi not connected\n", millis());
      return 0;
    }
    HTTPClient http; http.begin(RELAY_UPDATE_URL);
    http.addHeader("Content-Type","application/json"); http.setTimeout(HTTP_TIMEOUT);
    String clean = uuid; clean.replace(" ","");
    String payload = "{\"mac_address\":\"" + DEVICE_MAC_ADDR + "\",\"serial_number\":\"" + DEVICE_SERIAL_NUM +
                     "\",\"last_ip\":\"" + WiFi.localIP().toString() + "\",\"last_uuid\":\"" + clean + "\"}";
    Serial.printf("[HTTP] %lums  Relay POST Ã¢â€ â€™ %s\n", millis(), RELAY_UPDATE_URL);
    Serial.printf("[HTTP] %lums  Payload: %s\n", millis(), payload.c_str());
    int code = http.POST(payload);
    String resp = http.getString();
    Serial.printf("[HTTP] %lums  Response: HTTP %d  Body: %s\n", millis(), code, resp.c_str());
    if (code < 0) {
      Serial.printf("[HTTP] %lums  Relay error detail: %s\n", millis(), HTTPClient::errorToString(code).c_str());
      logNetworkSnapshot("Relay POST failure", RELAY_UPDATE_URL);
    }
    if (code == 200 || code == 201) playSuccessTone();
    else playErrorTone();
    http.end();
    logger.log("Relay: HTTP " + String(code));
    return code;
  }

  bool sendCombinedCompletionRequest(String difficulty) {
    flashScreen(TFT_BLACK);
    drawStatusIcon(ICON_INFO);
    drawAlertBorder(TFT_WHITE);
    int cx = M5Dial.Display.width() / 2;
    uint16_t gtaGold = M5Dial.Display.color565(218,165,32);
    M5Dial.Display.setTextDatum(MC_DATUM);

    // Match mission-success style: outlined title with gold fill.
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_DARKGREY);
    for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) if (dx || dy) M5Dial.Display.drawString("SENDING", cx + dx, 102 + dy);
    M5Dial.Display.setTextColor(gtaGold);
    M5Dial.Display.drawString("SENDING", cx, 102);

    M5Dial.Display.setTextColor(TFT_DARKGREY);
    for (int dx = -2; dx <= 2; dx++) for (int dy = -2; dy <= 2; dy++) if (dx || dy) M5Dial.Display.drawString("TO SERVER", cx + dx, 130 + dy);
    M5Dial.Display.setTextColor(gtaGold);
    M5Dial.Display.drawString("TO SERVER", cx, 130);

    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(TFT_SILVER);
    M5Dial.Display.drawString("Please hold still", cx, 150);
    for (int i = 0; i < 3; i++) {
      String dots = "";
      for (int d = 0; d <= i; d++) dots += ".";
      M5Dial.Display.fillRect(90, 165, 60, 20, TFT_BLACK);
      M5Dial.Display.setTextSize(3);
      M5Dial.Display.setTextColor(TFT_WHITE);
      M5Dial.Display.drawString(dots, cx, 175);
      playSendingDot(i);
      delay(300);
    }
    M5Dial.Display.setTextSize(2);
    M5Dial.Speaker.stop();

    if (!connectToWiFi()) { displayError(DisplayText::WIFI_FAILED); shutdownWiFi(); return false; }

    String fd = difficulty;
    if (fd.isEmpty() || fd == "None") {
      fd = "Easy";
    }
    if (playerCount == 0) { displayError("No Players Registered"); shutdownWiFi(); return false; }

    String primary = removeSpaces(playerUUIDs[0]);
    String uuids = "[";
    for (int i = 1; i < playerCount; i++) { if (i > 1) uuids += ","; uuids += "\"" + removeSpaces(playerUUIDs[i]) + "\""; }
    uuids += "]";
    String post;
    if (COMPLETION_INCLUDE_GAME_ID) {
      post = "{\"mac_address\":\"" + DEVICE_MAC_ADDR + "\",\"serial_number\":\"" + DEVICE_SERIAL_NUM +
             "\",\"game_id\":\"" + DEVICE_GAME_ID + "\",\"difficulty\":\"" + fd + "\",\"uuid\":\"" + primary + "\",\"uuids\":" + uuids + "}";
    } else {
      post = "{\"mac_address\":\"" + DEVICE_MAC_ADDR + "\",\"serial_number\":\"" + DEVICE_SERIAL_NUM +
             "\",\"difficulty\":\"" + fd + "\",\"uuid\":\"" + primary + "\",\"uuids\":" + uuids + "}";
    }
    logger.log("Completion: " + fd + " | " + String(playerCount) + " players");
    logger.log("Completion endpoint: " + API_ENDPOINT);
    String completionPayloadLog = post;
    if (completionPayloadLog.length() > 160) completionPayloadLog = completionPayloadLog.substring(0, 160) + "...";
    logger.log("Completion payload: " + completionPayloadLog);
    Serial.printf("[HTTP] %lums  Completion POST -> %s\n", millis(), API_ENDPOINT.c_str());
    Serial.printf("[HTTP] %lums  Payload: %s\n", millis(), post.c_str());

    HTTPClient http; http.setTimeout(HTTP_TIMEOUT); http.begin(API_ENDPOINT);
    http.addHeader("Content-Type","application/json");
    int code = -1; String resp;
    for (int r = 0; r < MAX_HTTP_RETRIES; r++) {
      Serial.printf("[HTTP] %lums  Completion attempt %d/%d\n", millis(), r + 1, MAX_HTTP_RETRIES);
      startSendHoldTone(); code = http.POST(post);
      resp = http.getString();
      Serial.printf("[HTTP] %lums  Response: HTTP %d  Body: %s\n", millis(), code, resp.c_str());
      if (code < 0) {
        Serial.printf("[HTTP] %lums  Completion error detail: %s\n", millis(), HTTPClient::errorToString(code).c_str());
        logNetworkSnapshot("Completion POST failure", API_ENDPOINT);
      } else if (isFullLogging()) {
        Serial.printf("[HTTP] %lums  Completion success-path snapshot: heap=%u rssi=%d\n", millis(), (unsigned)ESP.getFreeHeap(), WiFi.RSSI());
      }
      String completionRespLog = resp;
      if (completionRespLog.length() > 160) completionRespLog = completionRespLog.substring(0, 160) + "...";
      logger.log("Completion response: HTTP " + String(code) + " | " + completionRespLog);
      if (code == 200 || code == 201) { logger.log("Completion OK"); break; }
      logger.log("Completion FAIL (HTTP " + String(code) + ")");
      if (r < MAX_HTTP_RETRIES - 1) { playSound(SND_SERVER_RETRY, SND_SERVER_RETRY_LEN, 200, "SERVER_RETRY", EXT_SERVER_RETRY); yield(); delay(1000); }
    }
    http.end();
    if (code != 200 && code != 201) { Serial.printf("[HTTP] %lums  Completion FAILED after all retries\n", millis()); displayError("Send Failed"); delay(3000); shutdownWiFi(); return false; }

    playSound(SND_MISSION_COMPLETE, SND_MISSION_COMPLETE_LEN, 255, "MISSION_COMPLETE", EXT_MISSION_COMPLETE);
    drawMissionPassedScreen();
    delay(2500);
    shutdownWiFi();
    return true;
  }

  // ============================================================
  // Tag identification
  // ============================================================
  String identifyTag(String tagUID) {
    for (int i = 0; i < NUM_COMPLETE_TAGS; i++) if (tagUID == COMPLETE_TAGS[i]) return "COMPLETION TAG #" + String(i+1);
    for (int i = 0; i < NUM_FULL_RESET_TAGS; i++) if (tagUID == FULL_RESET_TAGS[i]) return "FULL RESET TAG #" + String(i+1);
    for (int i = 0; i < NUM_RESET_TAGS; i++) if (tagUID == RESET_TAG[i]) return "RESET TAG #" + String(i+1);
    for (int i = 0; i < NUM_BADGE_RESET_TAGS; i++) if (tagUID == BADGE_RESET_TAGS[i]) return "BADGE RESET TAG #" + String(i+1);
    for (int i = 0; i < NUM_WIFI_CONFIG_TAGS; i++) if (tagUID == WIFI_CONFIG_TAGS[i]) return "WIFI CONFIG TAG #" + String(i+1);
    for (int i = 0; i < NUM_ADMIN_BADGE_TAGS; i++) if (tagUID == ADMIN_BADGE_TAGS[i]) return "ADMIN BADGE #" + String(i+1);
    for (int i = 0; i < NUM_MISSION_CARD_TAGS; i++) if (!MISSION_CARD_TAGS[i].isEmpty() && tagUID == MISSION_CARD_TAGS[i]) return "MISSION CARD\\nHEIST #" + String(i+1);
    for (int i = 0; i < TOTAL_LOCATIONS; i++) {
      const LocationInfo& loc = POI_LOCATIONS[i];
      for (int j = 0; j < 3; j++) { if (!loc.weaponTags[j].isEmpty()   && tagUID == loc.weaponTags[j])   return "LOCATION\\n" + String(loc.name) + "\\nWEAPONS"; }
      for (int j = 0; j < 3; j++) { if (!loc.securityTags[j].isEmpty() && tagUID == loc.securityTags[j]) return "LOCATION\\n" + String(loc.name) + "\\nSECURITY"; }
      for (int j = 0; j < 3; j++) { if (!loc.vehicleTags[j].isEmpty()  && tagUID == loc.vehicleTags[j])  return "LOCATION\\n" + String(loc.name) + "\\nVEHICLE"; }
      for (int j = 0; j < 3; j++) { if (!loc.moneyTags[j].isEmpty()    && tagUID == loc.moneyTags[j])    return "LOCATION\\n" + String(loc.name) + "\\nMONEY"; }
    }
    return "UNKNOWN TAG\\n" + tagUID;
  }

  // ============================================================
  // State management helpers
  // ============================================================
  void resetBadgeOnly() {
    Serial.printf("[ACTION] %lums  resetBadgeOnly\n", millis());
    if (currentMission) { delete currentMission; currentMission = nullptr; }
    for (int i = 0; i < MAX_PLAYERS; i++) playerUUIDs[i] = "";
    playerCount = 0; waitingForBadge = true; trackerState = WAIT_FOR_BADGE;
    stateManager.setPlayerUUID(""); stateManager.setSelectedLocations(""); stateManager.setVisitedCount(0);
    missionStartTime = 0; lastTimerUpdate = 0; missionLocked = false;
    awaitingCompletionScan = false; lockedDifficulty = ""; completionPromptShown = false;
    stateManager.clearMissionStartTime(); stateManager.clearLockedDifficulty();
    displayScanBadge();
  }

  void resetBadgeForReuse() {
    Serial.printf("[ACTION] %lums  resetBadgeForReuse (saving state: %s, mission: %s)\n", millis(), stateName(trackerState), currentMissionCardUID.c_str());
    savedTrackerState = trackerState;
    savedMissionCardUID = currentMissionCardUID;
    stateManager.setSavedTrackerState(savedTrackerState);
    stateManager.setSavedMissionStart(missionStartTime);
    currentBadgeUID = "";
    for (int i = 0; i < MAX_PLAYERS; i++) playerUUIDs[i] = "";
    playerCount = 0; stateManager.setPlayerUUID("");
    awaitingBadgeRestore = true; trackerState = WAIT_FOR_BADGE; waitingForBadge = true;
    playSound(SND_RESET, SND_RESET_LEN, 255, "RESET", EXT_RESET);
    displayMessage(DisplayText::BADGE_RESET, COLOR_INFO, 1500);
    displayScanBadge();
  }

  void fullReset() {
    Serial.printf("[ACTION] %lums  fullReset Ã¢â‚¬â€ clearing all data\n", millis());
    if (currentMission) { delete currentMission; currentMission = nullptr; }
    playSound(SND_RESET, SND_RESET_LEN, 255, "RESET", EXT_RESET);
    displayMessage(DisplayText::FULL_RESET, COLOR_WARNING, 2000);
    stateManager.clearAll(); stateManager.clearCompletedBadges();
    stateManager.clearSavedTrackerState(); stateManager.clearSavedMissionStart();
    stateManager.clearMissionStartTime(); stateManager.clearLockedDifficulty();
    currentBadgeUID = ""; currentMissionCardUID = "";
    lastCompletedBadgeUIDs.clear(); badgeCooldownMap.clear();
    lastCompletedMissionCardUID = ""; missionLocked = false;
    awaitingCompletionScan = false; completionPromptShown = false; lockedDifficulty = "";
    for (int i = 0; i < MAX_PLAYERS; i++) playerUUIDs[i] = "";
    playerCount = 0;
    relayBadgeUID = ""; relayLastResponse = ""; relayLastSuccess = false;
    adminInMenu = true; adminMenuSelection = 0;
    awaitingBadgeRestore = false; savedTrackerState = -1; savedMissionCardUID = "";
    stateManager.setSelectedLocations(""); stateManager.setVisitedCount(0);
    logger.clearLog();
    displayMessage(DisplayText::MEMORY_CLEARED, COLOR_WARNING, 2000);
    displaySplashScreen();
    storyNpcToken = "";
    if (currentMode == MODE_RELAY) {
      trackerState = RELAY_WAIT_BADGE;
      displayRelayBadgePrompt();
    } else {
      trackerState = WAIT_FOR_NPC_TOKEN;
      waitingForBadge = false;
      displayScanNpcToken();
    }
  }

  void reset(bool bypassCooldown = true) {
    Serial.printf("[ACTION] %lums  reset (bypassCooldown=%s)\n", millis(), bypassCooldown ? "true" : "false");
    if (currentMission) { delete currentMission; currentMission = nullptr; }
    for (int i = 0; i < MAX_PLAYERS; i++) playerUUIDs[i] = "";
    playerCount = 0; waitingForBadge = false;
    currentBadgeUID = ""; currentMissionCardUID = "";
    lastCompletedBadgeUIDs.clear(); lastCompletedMissionCardUID = "";
    badgeCooldownMap.clear();
    String assignedNpcToken = stateManager.getStoryNpcToken();
    stateManager.clearAll(); stateManager.clearCompletedBadges();
    if (assignedNpcToken.length() > 0) stateManager.setStoryNpcToken(assignedNpcToken);
    stateManager.setSelectedLocations(""); stateManager.setVisitedCount(0);
    displayMessage(DisplayText::RESETTING, COLOR_INFO, 500);
    displaySplashScreen();
    if (assignedNpcToken.length() > 0) {
      storyNpcToken = normalizeRfidToken(assignedNpcToken);
      trackerState = WAIT_FOR_BADGE;
      displayScanBadge();
    } else {
      storyNpcToken = "";
      trackerState = WAIT_FOR_NPC_TOKEN;
      displayScanNpcToken();
    }
  }

  bool isBadgeAlreadyCompleted(String uid) {
    unsigned long cd = devMode ? 10000UL : BADGE_COOLDOWN_MS;
    auto it = badgeCooldownMap.find(uid);
    if (it != badgeCooldownMap.end()) {
      if (millis() - it->second < cd) return true;
      badgeCooldownMap.erase(it);
    }
    return false;
  }

  unsigned long getBadgeCooldownSecondsRemaining(const String& uid) {
    unsigned long cd = devMode ? 10000UL : BADGE_COOLDOWN_MS;
    auto it = badgeCooldownMap.find(uid);
    if (it != badgeCooldownMap.end()) {
      unsigned long e = millis() - it->second;
      if (e < cd) return (cd - e) / 1000UL;
    }
    return 0;
  }

  void markBadgesAsCompleted() {
    lastCompletedBadgeUIDs.clear();
    unsigned long now = millis();
    for (int i = 0; i < playerCount; i++) {
      if (playerUUIDs[i].length() > 0) { badgeCooldownMap[playerUUIDs[i]] = now; lastCompletedBadgeUIDs.push_back(playerUUIDs[i]); }
    }
  }

  int getMissionCategoryFromTag(String uid) {
    for (int i = 0; i < NUM_MISSION_CARD_TAGS; i++) {
      if (!MISSION_CARD_TAGS[i].isEmpty() && uid == MISSION_CARD_TAGS[i]) return 0;
    }
    for (int i = 0; i < NUM_FREE_ROAM_MISSION_TAGS; i++) {
      if (!FREE_ROAM_MISSION_TAGS[i].isEmpty() && uid == FREE_ROAM_MISSION_TAGS[i]) return 0;
    }
    return -1;
  }

private:
  bool waitingForBadge = false;
  bool wifiConfigAwaitingSSID = false, wifiConfigAwaitingPassword = false;
  String wifiConfigSSID;

  static const int MAX_PLAYERS = 10;
  String playerUUIDs[MAX_PLAYERS];
  int playerCount = 0;
  int lastEncoderValue = 0;

  MissionBase* currentMission = nullptr;
  String currentBadgeUID, currentMissionCardUID;
  std::vector<String> lastCompletedBadgeUIDs;
  String lastCompletedMissionCardUID;
  std::map<String, unsigned long> badgeCooldownMap;
  std::set<String> scrubbedBadgeUIDs;

  int savedTrackerState = -1;
  String savedMissionCardUID;
  bool awaitingBadgeRestore = false;

  unsigned long missionStartTime = 0, lastTimerUpdate = 0;
  bool missionLocked = false, awaitingCompletionScan = false;
  String lockedDifficulty;
  bool completionPromptShown = false;
  bool showingTimerOverlay = false;
  unsigned long timerOverlayStartTime = 0;

  String relayBadgeUID;
  String relayLastResponse;
  String storyNpcToken;
  bool relayLastSuccess = false;
  unsigned long lastActivityMs = 0;
  unsigned long lastWiFiReconnectAttemptMs = 0;
  unsigned long _splashAudioStartMs = 0;
  bool screenOn = true;
  static const unsigned long SCREEN_TIMEOUT_MS = 30000UL;
  static const unsigned long WIFI_BACKGROUND_RECONNECT_INTERVAL_MS = 15000UL;

  int confirmSelection = 0;
  long confirmEncoderPos = 0;

  // Ã¢â€â‚¬Ã¢â€â‚¬ Private helpers Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬

  static String removeSpaces(String s) { s.replace(" ",""); return s; }

  static String normalizeRfidToken(String token) {
    token.trim();
    token.replace(" ", "");
    token.replace("-", "");
    token.replace(":", "");
    token.toUpperCase();
    return token;
  }

  void persistPlayerList() {
    String csv;
    for (int i = 0; i < playerCount; i++) { if (i > 0) csv += ","; csv += playerUUIDs[i]; }
    stateManager.setPlayerUUID(csv);
  }

  // Start a mission with common timer init
  void startMission(MissionBase* m, void (*configure)(MissionBase*)) {
    if (currentMission) delete currentMission;
    currentMission = m;
    if (configure) configure(currentMission);
    startMissionTimer();
    currentMission->setup();
    trackerState = RUN_MISSION;
    currentMission->updateDisplay();
  }

  String normalizeStoryPoiName(String value) {
    value.toLowerCase();
    String normalized;
    for (size_t i = 0; i < value.length(); i++) {
      char c = value[i];
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) normalized += c;
    }
    return normalized;
  }

  int findStoryPoiIndex(const String& name) {
    String wanted = normalizeStoryPoiName(name);
    for (int i = 0; i < TOTAL_LOCATIONS; i++) {
      if (wanted == normalizeStoryPoiName(String(LOCATION_NAMES[i]))) return i;
      if (wanted == normalizeStoryPoiName(String(POI_LOCATIONS[i].name))) return i;
    }
    return -1;
  }

  bool storyPoiContainsUuid(int locIdx, const String& wantedUuid) {
    if (locIdx < 0 || locIdx >= TOTAL_LOCATIONS || wantedUuid.length() == 0) return false;
    const LocationInfo& loc = POI_LOCATIONS[locIdx];
    const String* tagSets[RESOURCE_COUNT] = {
      loc.weaponTags, loc.securityTags, loc.vehicleTags, loc.moneyTags
    };
    for (int res = 0; res < RESOURCE_COUNT; res++) {
      for (int t = 0; t < 3; t++) {
        const String normalized = normalizeRfidToken(tagSets[res][t]);
        if (normalized.length() > 0 && normalized == wantedUuid) return true;
      }
    }
    return false;
  }

  int findStoryPoiIndexByUuid(const String& uuid) {
    const String wanted = normalizeRfidToken(uuid);
    if (wanted.length() == 0) return -1;
    for (int i = 0; i < TOTAL_LOCATIONS; i++) {
      if (storyPoiContainsUuid(i, wanted)) return i;
    }
    return -1;
  }

  void recoverFromStoryRoundError(const String& reasonCode, bool automaticFreeRoam) {
    logger.log(String("Story round unavailable: ") + reasonCode);
    Serial.printf("[STORY] Recovering from story round error: %s\n", reasonCode.c_str());

    shutdownWiFi();

    if (currentMission) {
      delete currentMission;
      currentMission = nullptr;
    }
    missionLocked = false;
    awaitingCompletionScan = false;
    completionPromptShown = false;
    currentMissionCardUID = "";
    lockedDifficulty = "";
    missionStartTime = 0;
    lastTimerUpdate = 0;
    stateManager.clearMissionStartTime();
    stateManager.clearLockedDifficulty();

    playErrorTone();
    if (reasonCode == "STORY_MISSION_NOT_READY") {
      displayMultiLineMessage("MISSION NOT READY", "TRY AFTER NPC START", COLOR_WARNING);
    } else if (reasonCode == "NO_ACTIVE_ROUND") {
      displayMultiLineMessage("NO ACTIVE ROUND", "START MINIGAME", COLOR_WARNING);
    } else if (reasonCode == "STORY_MISSION_POI_COUNT_MISMATCH") {
      displayMultiLineMessage("MISSION NEEDS", "MORE POIS", COLOR_WARNING);
    } else if (reasonCode == "STORY_MISSION_POI_UUID_MISSING") {
      displayMultiLineMessage("POI TAG UUID", "MISSING", COLOR_WARNING);
    } else if (reasonCode == "STORY_NPC_NOT_CONFIGURED") {
      displayMultiLineMessage("NPC NOT LINKED", "CHECK CREATOR", COLOR_WARNING);
    } else if (reasonCode == "MISSION_UUID_NOT_FOUND") {
      displayMultiLineMessage("NPC TAG", "NOT FOUND", COLOR_WARNING);
    } else if (reasonCode == "DEVICE_OR_ROUTE_NOT_FOUND") {
      displayMultiLineMessage("DEVICE ROUTE", "NOT FOUND", COLOR_WARNING);
    } else if (reasonCode == "ROUND_NOT_READY") {
      displayMultiLineMessage("ROUND NOT", "READY", COLOR_WARNING);
    } else {
      displayMultiLineMessage("STORY FETCH", "FAILED", COLOR_WARNING);
    }
    delay(1800);

    if (automaticFreeRoam) {
      if (playerCount > 0) {
        confirmSelection = 0;
        confirmEncoderPos = M5Dial.Encoder.read();
        trackerState = CONFIRM_PLAYERS;
        waitingForBadge = false;
        displayConfirmPlayers();
      } else {
        trackerState = WAIT_FOR_BADGE;
        waitingForBadge = true;
        displayScanBadge();
      }
    } else {
      trackerState = WAIT_FOR_MISSION_CARD;
      waitingForBadge = false;
      displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
    }
  }

  void startStoryRoundFlow(bool showSplash, const String& missionUuid) {
    // Mission Widget keeps the Free Roam presentation, but the assigned NPC
    // token selects the server-generated shared mission for the active round.
    const bool automaticFreeRoam = currentMode == MODE_MISSION_WIDGET;
    if (playerCount == 0) {
      displayError("NO PLAYERS");
      delay(1500);
      displayScanBadge();
      return;
    }

    if (automaticFreeRoam) {
      Serial.printf("[FREE ROAM] %lums  Requesting current NPC round mission\n", millis());
    } else {
      Serial.printf("[STORY] %lums  Requesting shared round mission for UUID %s\n", millis(), missionUuid.c_str());
    }

    // Build and log the complete request before WiFi so connection failures do
    // not hide what the dial intended to send.
    DynamicJsonDocument request(2048);
    request["mac_address"] = DEVICE_MAC_ADDR;
    request["serial_number"] = DEVICE_SERIAL_NUM;
    request["game_id"] = DEVICE_GAME_ID;
    if (missionUuid.length() > 0) {
      request["mission_uuid"] = missionUuid;
      request["source_uuid"] = missionUuid;
    }
    request["npc_id"] = STORY_NPC_ID;
    JsonArray players = request.createNestedArray("player_uuids");
    for (int i = 0; i < playerCount; i++) players.add(removeSpaces(playerUUIDs[i]));
    String payload;
    serializeJson(request, payload);
    Serial.printf("[STORY JSON REQUEST] %s\n", payload.c_str());
    Serial.printf("[STORY TARGET] %s\n", STORY_ROUND_ENDPOINT);

    if (showSplash) {
      Serial.printf("[STORY] %lums  Showing mission start screen\n", millis());
      drawMissionStartingScreen();
      delay(900);
    }
    Serial.printf("[STORY] %lums  Connecting to WiFi for round manifest\n", millis());
    if (!connectToWiFi()) {
      displayError(DisplayText::WIFI_FAILED);
      delay(1500);
      if (automaticFreeRoam) displayScanBadge();
      else displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
      shutdownWiFi();
      return;
    }

    Serial.printf("[STORY] %lums  Sending previously logged JSON request\n", millis());

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT);
    http.setConnectTimeout(HTTP_TIMEOUT);
    http.begin(STORY_ROUND_ENDPOINT);
    http.addHeader("Content-Type", "application/json");
    unsigned long requestStartedAt = millis();
    int code = http.POST(payload);
    String response = code > 0 ? http.getString() : "";
    http.end();

    Serial.printf("[STORY] %lums  Manifest response HTTP %d in %lums (%d bytes)\n",
                  millis(), code, millis() - requestStartedAt, response.length());
    if (response.length() > 0) {
      logLongSerial("[STORY RAW RESPONSE]", response);
    }
    if (code < 0) {
      Serial.printf("[STORY] HTTP error detail: %s\n", HTTPClient::errorToString(code).c_str());
      logNetworkSnapshot("Story round POST failure", STORY_ROUND_ENDPOINT);
    }
    if (code != 200) {
      String serverCode;
      String serverMessage;
      if (response.length() > 0) {
        DynamicJsonDocument errorBody(8192);
        if (!deserializeJson(errorBody, response)) {
          serverCode = errorBody["code"].as<String>();
          serverMessage = errorBody["message"].as<String>();
          JsonObject details = errorBody["details"].as<JsonObject>();
          if (!details.isNull()) {
            const char* activeRoundKey = details["active_round_key"] | "";
            const char* activeRoundName = details["active_round_name"] | "";
            const char* npcId = details["npc_id"] | "";
            const char* npcName = details["npc_name"] | "";
            const char* npcPublicId = details["npc_public_id"] | "";
            JsonArray generatedForRound = details["generated_for_round"].as<JsonArray>();
            Serial.printf("[STORY ERROR DETAILS] active_round=%s (%s) npc=%s public=%s id=%s generated_for_round=%u\n",
                          activeRoundKey, activeRoundName, npcName, npcPublicId, npcId,
                          (unsigned)generatedForRound.size());
          }
        }
      }
      Serial.printf("[STORY] Server rejection: %s | %s\n", serverCode.c_str(), serverMessage.c_str());
      logger.log(String(automaticFreeRoam ? "Free roam round manifest failed HTTP " : "Story manifest failed HTTP ") + String(code));
      String recoveryCode = serverCode;
      if (recoveryCode.length() == 0) {
        if (code == 404) recoveryCode = "DEVICE_OR_ROUTE_NOT_FOUND";
        else if (code == 409) recoveryCode = "ROUND_NOT_READY";
        else recoveryCode = "HTTP_ERROR";
      }
      recoverFromStoryRoundError(recoveryCode, automaticFreeRoam);
      return;
    }

    DynamicJsonDocument manifest(12288);
    DeserializationError parseError = deserializeJson(manifest, response);
    if (parseError || !manifest["ok"].as<bool>()) {
      Serial.printf("[STORY] Manifest parse/error: %s\n", parseError ? parseError.c_str() : "ok=false");
      logger.log("Story manifest parse failed");
      displayError("BAD STORY DATA");
      delay(1800);
      if (automaticFreeRoam) displayScanBadge();
      else displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
      shutdownWiFi();
      return;
    }

    JsonObject deviceCtx = manifest["device"].as<JsonObject>();
    JsonObject sourceCtx = manifest["source"].as<JsonObject>();
    JsonObject roundCtx = manifest["round"].as<JsonObject>();
    JsonObject missionCtx = manifest["mission"].as<JsonObject>();
    JsonObject dialInit = manifest["dial_init"].as<JsonObject>();
    JsonObject debugCtx = manifest["debug"].as<JsonObject>();
    JsonArray missingUuidPois = debugCtx["missing_uuid_pois"].as<JsonArray>();
    Serial.printf("[STORY CONTEXT] requested_game=%s resolved_game=%s game_match=%s widget=%s active_minigame=%s\n",
                  (const char*)(deviceCtx["requested_game_id"] | ""),
                  (const char*)(deviceCtx["resolved_game_id"] | ""),
                  (bool)(deviceCtx["game_id_matches_request"] | false) ? "true" : "false",
                  (const char*)(deviceCtx["widget_id"] | ""),
                  (const char*)(deviceCtx["active_minigame_id"] | ""));
    Serial.printf("[STORY CONTEXT] mission_uuid=%s source_uuid=%s npc=%s public=%s npc_id=%s story_keyword=%s\n",
                  (const char*)(sourceCtx["mission_uuid"] | ""),
                  (const char*)(sourceCtx["source_uuid"] | ""),
                  (const char*)(sourceCtx["npc_name"] | ""),
                  (const char*)(sourceCtx["npc_public_id"] | ""),
                  (const char*)(sourceCtx["npc_id"] | ""),
                  (const char*)(sourceCtx["story_keyword"] | ""));
    Serial.printf("[STORY CONTEXT] round=%s name=%s type=%s mission=%s chain=%s\n",
                  (const char*)(roundCtx["round_key"] | ""),
                  (const char*)(roundCtx["name"] | ""),
                  (const char*)(roundCtx["type"] | ""),
                  (const char*)(missionCtx["name"] | ""),
                  (const char*)(missionCtx["source_chain_id"] | ""));
    Serial.printf("[STORY CONTEXT] dial_required=%d dial_max=%d variable=%s missing_uuid_pois=%u\n",
                  (int)(dialInit["required_poi_count"] | 0),
                  (int)(dialInit["max_supported_poi_count"] | 0),
                  (bool)(dialInit["variable_poi_count"] | false) ? "true" : "false",
                  (unsigned)missingUuidPois.size());

    JsonArray pois = manifest["pois"].as<JsonArray>();
    int requiredPoiCount = manifest["dial_init"]["required_poi_count"] | (int)pois.size();
    if (requiredPoiCount < 1 || requiredPoiCount > REQUIRED_LOCATIONS || (int)pois.size() < requiredPoiCount) {
      Serial.printf("[STORY] Expected 1-%d POIs, manifest requires %d, received %d\n",
                    REQUIRED_LOCATIONS, requiredPoiCount, (int)pois.size());
      displayError(requiredPoiCount > REQUIRED_LOCATIONS ? "TOO MANY POIS" : "NO POIS");
      delay(1800);
      if (automaticFreeRoam) displayScanBadge();
      else displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
      shutdownWiFi();
      return;
    }

    String selected = "v4|C=" + String(requiredPoiCount) + "|S=";
    String selectedNames = "";
    for (int i = 0; i < requiredPoiCount; i++) {
      if (i > 0) {
        selected += ",";
        selectedNames += "~";
      }
      String poiName = pois[i]["name"].as<String>();
      String poiUuid = pois[i]["uuid"].as<String>();
      String displayName = poiName;
      displayName.replace("|", " ");
      displayName.replace("~", " ");
      displayName.trim();
      int localIndex = findStoryPoiIndexByUuid(poiUuid);
      if (localIndex < 0) {
        localIndex = findStoryPoiIndex(poiName);
      }
      if (localIndex < 0) {
        Serial.printf("[STORY] POI is not mapped on this dial: %s (uuid=%s)\n", poiName.c_str(), poiUuid.c_str());
        displayError("POI NOT ON DIAL");
        delay(1800);
        if (automaticFreeRoam) displayScanBadge();
        else displayMultiLineMessage("STORY ROUND", "SCAN MISSION UUID", COLOR_INFO);
        shutdownWiFi();
        return;
      }
      if (displayName.length() == 0) displayName = String(LOCATION_NAMES[localIndex]);
      selected += String(localIndex);
      selectedNames += displayName;
      Serial.printf("[STORY] POI %d/%d: %s (uuid=%s) -> local %d\n",
                    i + 1, requiredPoiCount, displayName.c_str(), poiUuid.c_str(), localIndex);
    }
    selected += "|N=" + selectedNames + "|V=";
    for (int i = 0; i < requiredPoiCount; i++) {
      if (i > 0) selected += ",";
      selected += "0";
    }
    stateManager.setSelectedLocations(selected);
    String resolvedMissionId = manifest["mission"]["id"].as<String>();
    String roundKey = manifest["round"]["round_key"].as<String>();
    currentMissionCardUID = missionUuid.length() > 0 ? missionUuid : resolvedMissionId;
    logger.log(String(automaticFreeRoam ? "Free roam loaded story round " : "Story round ") + roundKey);
    String returnedMissionName = manifest["mission"]["name"].as<String>();
    String returnedNpcName = manifest["source"]["npc_name"].as<String>();
    Serial.printf("[%s] Mission: %s | NPC: %s | Round: %s\n",
                  automaticFreeRoam ? "FREE ROAM" : "STORY",
                  returnedMissionName.c_str(), returnedNpcName.c_str(),
                  roundKey.c_str());

    shutdownWiFi();
    playSound(SND_MISSION_START, SND_MISSION_START_LEN, 255, "MISSION_START", EXT_MISSION_START);
    startMission(new FreeRoamMission(stateManager), nullptr);
  }

  void startFreeRoamFlow(bool showSplash) {
    // Keep Free Roam's display and gameplay, but seed it from this dial's
    // one-time NPC assignment and the shared mission for the active round.
    String token = normalizeRfidToken(stateManager.getStoryNpcToken());
    if (token.length() == 0) {
      Serial.printf("[NPC] %lums  No story NPC token assigned\n", millis());
      trackerState = WAIT_FOR_NPC_TOKEN;
      waitingForBadge = false;
      displayScanNpcToken();
      return;
    }
    storyNpcToken = token;
    Serial.printf("[ACTION] %lums  Starting Free Roam for NPC token %s\n", millis(), storyNpcToken.c_str());
    startStoryRoundFlow(showSplash, storyNpcToken);
  }

  void startMissionTimer() {
    missionStartTime = millis(); lastTimerUpdate = 0;
    missionLocked = false; awaitingCompletionScan = false; lockedDifficulty = "";
    stateManager.setMissionStartTime(missionStartTime);
    stateManager.clearLockedDifficulty();
    if (currentMission) currentMission->setMissionStartMs(missionStartTime);
  }

  void executeAdminMenuItem() {
    Serial.printf("[ACTION] %lums  Admin menu item selected: %d\n", millis(), adminMenuSelection);
    switch (adminMenuSelection) {
      case 0: displayDeviceInfo(); logger.log("Admin: device info"); break;
      case 1:
        currentMode = MODE_MISSION_WIDGET; stateManager.setOperationalMode(currentMode);
        displayMessage("Switched to\\nMission Mode", COLOR_SUCCESS, 1500);
        logger.log("Admin: Mission Widget");
        exitAdminMode();
        break;
      case 2:
        currentMode = MODE_STORY_MISSION_WIDGET; stateManager.setOperationalMode(currentMode);
        displayMessage("Switched to\\nStory Round", COLOR_SUCCESS, 1500);
        logger.log("Admin: Story Round");
        exitAdminMode();
        break;
      case 3:
        currentMode = MODE_RELAY; stateManager.setOperationalMode(currentMode);
        trackerState = RELAY_WAIT_BADGE;
        displayMessage("Switched to\\nRelay Mode", COLOR_SUCCESS, 1500);
        logger.log("Admin: Relay"); displayRelayBadgePrompt(); break;
      case 4:
        devMode = !devMode;
        displayMessage(devMode ? DisplayText::ADMIN_DEV_MODE_ON : DisplayText::ADMIN_DEV_MODE_OFF,
                       devMode ? COLOR_SUCCESS : COLOR_INFO, 1500);
        logger.log("Admin: devMode=" + String(devMode));
        displayAdminMode(); break;
      case 5:
        adminInMenu = false; logger.log("Admin: view log");
        displayLogFile(); adminInMenu = true; displayAdminMode(); break;
      case 6:
        adminInMenu = false; logger.log("Admin: scan tag");
        displayMessage("Scan Any Tag", COLOR_INFO, 0); break;
      case 7:
        stateManager.clearStoryNpcToken();
        storyNpcToken = "";
        logger.log("Admin: NPC token cleared");
        displayMessage("NPC TAG\nCLEARED", COLOR_WARNING, 1500);
        trackerState = WAIT_FOR_NPC_TOKEN;
        adminInMenu = true;
        displayScanNpcToken();
        break;
      case 8: exitAdminMode(); break;
    }
  }

  void drawTimerExpiredAnimation() {
    static M5Canvas canvas(&M5Dial.Display);
    static bool ready = false;
    if (!ready) { canvas.createSprite(240, 140); ready = true; }

    const int cx = 120;
    const int barX = 20;
    const int barY = 112;
    const int barW = 200;
    const int segments = 10;
    const int gap = 3;
    const int segmentW = (barW - (segments - 1) * gap) / segments;

    for (int frame = 0; frame < 10; frame++) {
      canvas.fillSprite((frame % 2) ? TFT_BLACK : TFT_MAROON);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextColor(TFT_RED);
      canvas.setTextSize(2);
      canvas.drawString("HEALTH DEPLETED", cx, 25);

      canvas.setTextColor(TFT_WHITE);
      canvas.setTextSize(5);
      canvas.drawString("0:00", cx, 67);

      canvas.setTextColor((frame % 2) ? TFT_ORANGE : TFT_YELLOW);
      canvas.setTextSize(2);
      canvas.drawString("TIME EXPIRED", cx, 94);

      for (int i = 0; i < segments; i++) {
        int x = barX + i * (segmentW + gap);
        canvas.drawRoundRect(x, barY, segmentW, 10, 2, TFT_RED);
      }
      canvas.drawRoundRect(5 + (frame % 2), 5 + (frame % 2),
                          230 - (frame % 2) * 2, 130 - (frame % 2) * 2,
                          8, (frame % 2) ? TFT_ORANGE : TFT_RED);
      canvas.pushSprite(0, 0);
      delay(90);
    }
  }

  void drawTimerOverlay(unsigned long now) {
    unsigned long elapsed = now - missionStartTime;
    unsigned long rem = (elapsed >= MISSION_TIMEOUT_MS) ? 0 : MISSION_TIMEOUT_MS - elapsed;
    unsigned long mins = rem / 60000, secs = (rem / 1000) % 60;
    char ts[8]; snprintf(ts, sizeof(ts), "%lu:%02lu", mins, secs);
    static M5Canvas canvas(&M5Dial.Display);
    static bool ready = false;
    if (!ready) { canvas.createSprite(240, 140); ready = true; }
    bool critical = rem <= 60000UL;
    bool urgent = rem <= 10000UL;
    bool pulse = urgent && ((now / 250UL) % 2UL == 0UL);
    float ratio = (MISSION_TIMEOUT_MS > 0UL) ? ((float)rem / (float)MISSION_TIMEOUT_MS) : 0.0f;
    canvas.fillSprite(pulse ? TFT_MAROON : TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    uint16_t healthColor = (ratio > 0.50f) ? TFT_GREEN : ((ratio > 0.25f) ? TFT_ORANGE : TFT_RED);
    canvas.setTextSize(2); canvas.setTextColor(healthColor);
    canvas.drawString(DisplayText::TIMER_TITLE, 120, 25);
    canvas.setTextSize(5); canvas.setTextColor(critical ? healthColor : TFT_WHITE);
    canvas.drawString(String(ts), 120, 67);
    if (critical) {
      canvas.setTextColor(urgent ? (pulse ? TFT_WHITE : TFT_YELLOW) : TFT_ORANGE);
      canvas.setTextSize(2);
      canvas.drawString(urgent ? "FINAL WARNING" : DisplayText::TIMER_HURRY, 120, 91);
    }

    int bx = 20, by2 = 116;
    int filledSegments = (int)(10.0f * ratio + 0.999f);
    if (filledSegments > 10) filledSegments = 10;
    for (int i = 0; i < 10; i++) {
      int sx = bx + i * 20;
      uint16_t segmentColor = (i < filledSegments) ? healthColor : TFT_DARKGREY;
      canvas.fillRoundRect(sx, by2, 17, 10, 2, segmentColor);
      canvas.drawRoundRect(sx, by2, 17, 10, 2, TFT_WHITE);
    }
    if (urgent) canvas.drawRoundRect(14, 108, 212, 26, 5, pulse ? TFT_WHITE : TFT_RED);
    canvas.pushSprite(0, 0);
  }
};

// ============================================================
// Global instance & Arduino setup/loop
// ============================================================
StoryTracker tracker;

unsigned long buttonPressStartTime = 0;
bool buttonHeldForAdmin = false;

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, true);
  M5Dial.Speaker.begin();
  Serial.begin(115200);
  delay(50);
  esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("[BOOT] Reset reason: %s (%d) heap=%u\n", resetReasonName(resetReason), (int)resetReason, (unsigned)ESP.getFreeHeap());

  if (isFullLogging()) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  } else if (isVerboseLogging()) {
    esp_log_level_set("*", ESP_LOG_INFO);
  } else {
    esp_log_level_set("*", ESP_LOG_WARN);
  }
  Serial.printf("[LOG] %lums  SERIAL_LOG_LEVEL=%d\n", millis(), SERIAL_LOG_LEVEL);

  randomSeed(analogRead(0) * millis());

  // Init Unit AudioPlayer on Port B (optional)
#if ENABLE_UNIT_AUDIO
  // M5Dial Port B: pin1=GPIO1=RX, pin2=GPIO2=TX  (same as original begin(&Serial1,1,2))
  int8_t portB_rx = M5.getPin(m5::pin_name_t::port_b_pin1);
  int8_t portB_tx = M5.getPin(m5::pin_name_t::port_b_pin2);
  Serial.printf("[BOOT] Port B pins: RX=%d TX=%d\n", portB_rx, portB_tx);

  {
    unsigned long audioInitStart = millis();
    bool audioOk = false;
    while (!audioOk) {
      audioOk = audioPlayer.begin(&Serial1, portB_rx, portB_tx);
      if (audioOk) break;
      Serial.println("[BOOT] Waiting for AudioPlayer...");
      if (millis() - audioInitStart >= 5000) {
        Serial.println("[BOOT] AudioPlayer init timed out Ã¢â‚¬â€ continuing without audio");
        break;
      }
      delay(1000);
    }
    if (audioOk) {
      audioPlayerReady = true;
      delay(500);  // give N9301 chip time to finish booting
      audioPlayer.setVolume(30);
      audioPlayer.setPlayMode(AUDIO_PLAYER_MODE_SINGLE_STOP);
      Serial.println("[BOOT] AudioPlayer ready on Port B");
    }
  }
#else
  audioPlayerReady = false;
  Serial.println("[BOOT] Unit AudioPlayer disabled via ENABLE_UNIT_AUDIO=0");
#endif

  // Register ext audio callback so Sounds.h can play .mp3 files
  setExtAudioPlayer([](uint16_t idx) -> bool {
    if (!audioPlayerReady) {
      Serial.printf("[AUDIO] EXT callback: audioPlayerReady=false, skipping idx %d\n", idx);
      return false;
    }
    // Flush stale async bytes (playback-status notifications) from UART RX buffer
    while (Serial1.available()) { Serial1.read(); }
    char fname[12]; snprintf(fname, sizeof(fname), "%03d.mp3", idx);
    audioPlayer.playAudioByName(String(fname));
    Serial.printf("[AUDIO] EXT sent playAudioByName(%s)\n", fname);
    return true;
  });

  Serial.printf("[BOOT] before tracker.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
  tracker.begin();
  Serial.printf("[BOOT] after tracker.begin heap=%u\n", (unsigned)ESP.getFreeHeap());
  Serial.printf("[BOOT] before restoreFromSavedSnapshot heap=%u\n", (unsigned)ESP.getFreeHeap());
  tracker.restoreFromSavedSnapshot();
  Serial.printf("[BOOT] after restoreFromSavedSnapshot heap=%u\n", (unsigned)ESP.getFreeHeap());
  tracker.bumpActivity();
}

void loop() {
  M5Dial.update();
  // NOTE: do NOT call audioPlayer.update() here Ã¢â‚¬â€ it races with
  // waitForResponse() inside playAudioByName, stealing response bytes

  // Activity detection
  {
    bool any = false;
    static long _lastEnc = 0;
    long enc = M5Dial.Encoder.read();
    if (enc != _lastEnc) { _lastEnc = enc; any = true; }
    if (M5Dial.BtnA.isPressed()) any = true;
    if (M5Dial.Touch.getDetail().wasPressed()) any = true;
    if (any) tracker.bumpActivity();
  }

  // State-change audio
  static TrackerState lastState = WAIT_FOR_BADGE;
  if (tracker.trackerState != lastState) {
    Serial.printf("[STATE] %lums  State changed: %s -> %s\n", millis(), stateName(lastState), stateName(tracker.trackerState));
    if (LOUD_MODE) {
      if (lastState == WAIT_FOR_BADGE && tracker.trackerState == CONFIRM_PLAYERS) playConfirmPlayersTone();
      else playStateChangeTone();
    }
    lastState = tracker.trackerState;
  }

  // Button hold (admin exit only)
  if (M5Dial.BtnA.isPressed()) {
    if (buttonPressStartTime == 0) buttonPressStartTime = millis();
    else if (!buttonHeldForAdmin && millis() - buttonPressStartTime >= ADMIN_BUTTON_HOLD_TIME) {
      buttonHeldForAdmin = true;
      if (tracker.trackerState == ADMIN_MODE) {
        if (!tracker.adminInMenu) { tracker.adminInMenu = true; tracker.adminPropUID = ""; tracker.displayAdminMode(); }
        else tracker.exitAdminMode();
      }
    }
  } else {
    if (buttonPressStartTime > 0 && !buttonHeldForAdmin) { delay(300); tracker.handleButtonPress(); }
    buttonPressStartTime = 0; buttonHeldForAdmin = false;
  }

  // Touch Ã¢â€ â€™ confirm players
  auto touch = M5Dial.Touch.getDetail();
  if (touch.wasPressed()) {
    if (tracker.trackerState == CONFIRM_PLAYERS) {
      if (tracker.handleConfirmPlayersTouch(touch.x, touch.y)) {
        delay(250);
      }
    } else if (tracker.trackerState == WAIT_FOR_BADGE && tracker.getPlayerCount() > 0) {
      tracker.advanceFromBadgeScreen(); delay(300);
    }
  }

  // RFID scan
  if (M5Dial.Rfid.PICC_IsNewCardPresent() && M5Dial.Rfid.PICC_ReadCardSerial()) {
    String cardUID = "";
    for (byte i = 0; i < M5Dial.Rfid.uid.size; i++) {
      cardUID += String(M5Dial.Rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      cardUID += String(M5Dial.Rfid.uid.uidByte[i], HEX);
    }
    cardUID.toUpperCase();
    String sanitized = cardUID; sanitized.replace(" ","");
    if (isOTAUpdateTag(sanitized)) { triggerGitHubUpdate(); return; }
    tracker.processCardScan(cardUID);
    tracker.bumpActivity();
    delay(1000);
  }

  tracker.update();
}
