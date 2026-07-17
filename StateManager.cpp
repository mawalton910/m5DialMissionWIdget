#include <Arduino.h>
#include "StateManager.h"

// Implementation for persistent session snapshot methods
int StateManager::getSavedTrackerState() {
    return prefs.getInt(this->KEY_SAVED_TRACKER_STATE, 0);
}

unsigned long StateManager::getSavedMissionStart() {
    return prefs.getULong(this->KEY_SAVED_MISSION_START, 0);
}

String StateManager::getLockedDifficulty() {
    return prefs.getString(this->KEY_LOCKED_DIFFICULTY, "");
}

void StateManager::setSavedMissionStart(unsigned long ms) {
    prefs.putULong(this->KEY_SAVED_MISSION_START, ms);
}

void StateManager::setLockedDifficulty(const String& diff) {
    prefs.putString(this->KEY_LOCKED_DIFFICULTY, diff.c_str());
}
