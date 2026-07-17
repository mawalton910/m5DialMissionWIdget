#pragma once
#include <Arduino.h>
#include "MissionBase.h"
#include "Config.h"
#include "StateManager.h"
#include <vector>

// ============================================================
// FreeRoamMission — "Free Roam" mode
//
// Players can visit any POI in any order. Scanning any RFID tag
// assigned to that POI counts as a visit for that location.
//
// There are no resource-type choices in this mode. Difficulty
// scales with the number of unique POIs visited before players
// scan the completion card.
// ============================================================
class FreeRoamMission : public MissionBase {
public:
    explicit FreeRoamMission(StateManager& sm);

    // MissionBase overrides
    void   setup() override;
    void   processLocation(String locationTag) override;
    void   updateDisplay() override;
    String getCurrentDifficulty() override;
    bool   isComplete() override { return visitedCount >= 1; }
    void   setMissionStartMs(unsigned long t) override { missionStartMs = t; }
    int    getVisitCount() const override { return visitedCount; }
    String getSelectedLocationsString() const override;
    std::vector<int> getCollectedResources() const override { return std::vector<int>(); }

private:
    StateManager& stateManager;

    bool poiVisited[TOTAL_LOCATIONS] = {};
    int missionLocations[REQUIRED_LOCATIONS] = {};
    bool missionSpotVisited[REQUIRED_LOCATIONS] = {};

    int  visitedCount    = 0;
    int  browseIndex     = 0;
    int  lastEncoder     = 0;
    unsigned long missionStartMs = 0;

    int findLocationForTag(const String& locationTag) const;
    bool parseSavedMissionState(const String& saved);
    void buildMissionSelection();
    int findDevSpotForTag(const String& locationTag) const;
    void drawProgressPips(int cx, int cy, int done, int activeIndex);
};
