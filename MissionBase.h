#pragma once
#include <Arduino.h>
 #include "Config.h"

// Abstract base class for missions
class MissionBase {
public:
    // Virtual destructor for proper cleanup
    virtual ~MissionBase() = 0;
    
    // Core mission functions
    virtual void setup() = 0;
    virtual void processLocation(String locationTag) = 0;
    virtual void updateDisplay() = 0;
    
    // Get current difficulty level
    virtual String getCurrentDifficulty() { return "None"; }

    // Returns true when at least one location has been visited (unlock completion tag)
    virtual bool isComplete() { return false; }

    // Sync the mission start timestamp for the always-on timer (ms from millis())
    virtual void setMissionStartMs(unsigned long) {}

    // State-persistence helpers used by restoreFromSavedSnapshot / snapshot save
    // Default no-op / 0 implementations allow non-Story missions to coexist safely
    virtual int getVisitCount() const { return 0; }
    virtual String getSelectedLocationsString() const { return ""; }
    
    // Get collected resources (default returns empty vector)
    virtual std::vector<int> getCollectedResources() const { return std::vector<int>(); }

    // Safe Cracker integration:
    // Try to match a tag to an unvisited location+resource without collecting it.
    // Returns true and fills outLocIdx / outResType if matched.
    virtual bool findResourceTag(String /*tag*/, int& /*outLocIdx*/, int& /*outResType*/) { return false; }

    // Credit a resource (called by Safe Cracker on win instead of processLocation).
    virtual void addResource(int /*locIdx*/, int /*resType*/) {}
};

// Implementation of pure virtual destructor
inline MissionBase::~MissionBase() {}