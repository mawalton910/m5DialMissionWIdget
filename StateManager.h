
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class StateManager {
public:
    void begin() {
        prefs.begin(NAMESPACE, false);
    }

    // Restore a snapshot of all session variables
    void restoreSessionSnapshot(String& badgeUID, String& missionCardUID, int& trackerState, int& visitedCount, String& selectedLocs, unsigned long& missionStartTime, String& lockedDifficulty) {
        badgeUID = getPlayerUUID();
        missionCardUID = prefs.getString("mission_card_uid", "");
        trackerState = getSavedTrackerState();
        visitedCount = getVisitedCount();
        selectedLocs = getSelectedLocations();
        missionStartTime = getSavedMissionStart();
        lockedDifficulty = getLockedDifficulty();
    }
    // Save a snapshot of all session variables
    void saveSessionSnapshot(const String& badgeUID, const String& missionCardUID, int trackerState, int visitedCount, const String& selectedLocs, unsigned long missionStartTime, const String& lockedDifficulty) {
        setPlayerUUID(badgeUID);
        prefs.putString("mission_card_uid", missionCardUID.c_str());
        setSavedTrackerState(trackerState);
        setVisitedCount(visitedCount);
        setSelectedLocations(selectedLocs);
        setSavedMissionStart(missionStartTime);
        setLockedDifficulty(lockedDifficulty);
    }
    Preferences prefs;
    const char* NAMESPACE = "story_mission";
    const char* COMPLETED_NAMESPACE = "completed";
        
    // Keys for different state variables
    const char* KEY_PLAYER_UUID = "player_uuid";
    const char* KEY_PLAYER_CLASS = "player_class";
    const char* KEY_VISITED_COUNT = "visited_count";
    const char* KEY_SELECTED_LOCS = "selected_locs";
    const char* KEY_SAVED_TRACKER_STATE = "saved_state";
    const char* KEY_SAVED_MISSION_START = "saved_ms_start";
    const char* KEY_MISSION_START_TIME = "mission_start_ms";
    const char* KEY_LOCKED_DIFFICULTY = "locked_difficulty";
    const char* KEY_WIFI_SSID = "wifi_ssid";
    const char* KEY_WIFI_PASSWORD = "wifi_password";
    const char* KEY_OPERATIONAL_MODE = "op_mode";
    // ...existing code...
    
    void clearAll() {
        prefs.clear();
    }
    
    // Clear the completed badges namespace
    void clearCompletedBadges() {
        prefs.end();
        prefs.begin(COMPLETED_NAMESPACE, false);
        prefs.clear();
        prefs.end();
        prefs.begin(NAMESPACE, false);
    }
    
    // Derive a safe NVS key from a badge UID: strips spaces, clamps to 15-char ESP32 NVS limit (B4).
    String nvsBadgeKey(const String& badge) {
        String key = badge;
        key.replace(" ", "");
        if (key.length() > 15) key = key.substring(key.length() - 15);
        return key;
    }

    // Mark a badge as having completed the mission
    void addCompletedBadge(const String& badge) {
        prefs.end();
        prefs.begin(COMPLETED_NAMESPACE, false);
        prefs.putBool(nvsBadgeKey(badge).c_str(), true);
        prefs.end();
        prefs.begin(NAMESPACE, false);
    }

    // Check if a badge has already completed the mission
    bool isBadgeCompleted(const String& badge) {
        prefs.end();
        prefs.begin(COMPLETED_NAMESPACE, false);
        bool result = prefs.getBool(nvsBadgeKey(badge).c_str(), false);
        prefs.end();
        prefs.begin(NAMESPACE, false);
        return result;
    }
        
    // Player UUID
    void setPlayerUUID(const String& uuid) {
        prefs.putString(KEY_PLAYER_UUID, uuid.c_str());
    }
    
    String getPlayerUUID() {
        return prefs.getString(KEY_PLAYER_UUID, "");
    }
    
    // Player Class
    void setPlayerClass(int playerClass) {
        prefs.putInt(KEY_PLAYER_CLASS, playerClass);
    }
    
    int getPlayerClass() {
        return prefs.getInt(KEY_PLAYER_CLASS, 0); // Default to SCOUT
    }
    
    // Location visited count
    void setVisitedCount(int count) {
        prefs.putInt(KEY_VISITED_COUNT, count);
    }
    
    int getVisitedCount() {
        return prefs.getInt(KEY_VISITED_COUNT, 0);
    }
    
    // Selected locations (stored as string)
    void setSelectedLocations(const String& locationsStr) {
        prefs.putString(KEY_SELECTED_LOCS, locationsStr.c_str());
    }
    
    String getSelectedLocations() {
        return prefs.getString(KEY_SELECTED_LOCS, "");
    }

    // Mission timer persistence
    void setMissionStartTime(unsigned long ms) {
        prefs.putULong(KEY_MISSION_START_TIME, ms);
    }

    unsigned long getMissionStartTime() {
        return prefs.getULong(KEY_MISSION_START_TIME, 0);
    }

    void clearMissionStartTime() {
        prefs.remove(KEY_MISSION_START_TIME);
    }

    void clearLockedDifficulty() {
        prefs.remove(KEY_LOCKED_DIFFICULTY);
    }

    void clearSavedTrackerState() {
        prefs.remove(KEY_SAVED_TRACKER_STATE);
    }

    void clearSavedMissionStart() {
        prefs.remove(KEY_SAVED_MISSION_START);
    }

    // WiFi credential persistence
    void setWiFiCredentials(const String& ssid, const String& password) {
        prefs.putString(KEY_WIFI_SSID, ssid.c_str());
        prefs.putString(KEY_WIFI_PASSWORD, password.c_str());
    }

    bool hasStoredWiFiCredentials() {
        String ssid = prefs.getString(KEY_WIFI_SSID, "");
        return ssid.length() > 0;
    }

    String getStoredWiFiSSID() {
        return prefs.getString(KEY_WIFI_SSID, "");
    }

    String getStoredWiFiPassword() {
        return prefs.getString(KEY_WIFI_PASSWORD, "");
    }

    // Backward-compatible aliases used by older sketch code
    String getWiFiSSID() {
        return getStoredWiFiSSID();
    }

    String getWiFiPassword() {
        return getStoredWiFiPassword();
    }

    // Persist a saved tracker state when doing temporary badge resets
    void setSavedTrackerState(int state) {
        prefs.putInt(KEY_SAVED_TRACKER_STATE, state);
    }

    // ...existing code...
    
    void clearWiFiCredentials() {
        prefs.remove(KEY_WIFI_SSID);
        prefs.remove(KEY_WIFI_PASSWORD);
    }
    
    // Operational Mode persistence (mission-only)
    void setOperationalMode(int mode) {
        prefs.putInt(KEY_OPERATIONAL_MODE, mode);
    }
    
    int getOperationalMode() {
        return prefs.getInt(KEY_OPERATIONAL_MODE, 0); // Default to MODE_MISSION_WIDGET (0)
    }
    
    void end() {
        prefs.end();
    }
    
        // Add missing public getter/setter declarations
        int getSavedTrackerState();
        unsigned long getSavedMissionStart();
        String getLockedDifficulty();
        void setSavedMissionStart(unsigned long ms);
        void setLockedDifficulty(const String& diff);
};