# How to Add a New Mission Type

This guide walks you through adding a new mission (mini-game) to the M5Dial system. No deep C++ expertise is required — just follow the steps and use the existing missions as templates.

---

## Quick Overview

Every mission is a class that implements the `MissionBase` interface. The main program (`mission_M5_refactored.ino`) handles RFID scanning, the state machine, and calling your mission's functions. You just write the game logic and display code.

**What you create:**
1. A `.h` header file (class declaration)
2. A `.cpp` source file (class implementation)
3. An RFID tag entry in `Config.h` (to trigger your mission)
4. A few lines in `mission_M5_refactored.ino` (to wire it up)

---

## Step 1: Understand MissionBase

All missions implement this interface (defined in `MissionBase.h`):

```cpp
class MissionBase {
public:
    virtual ~MissionBase() = 0;

    // REQUIRED — you must implement these 3:
    virtual void setup() = 0;                         // Initialize your mission
    virtual void processLocation(String locationTag) = 0;  // Handle an RFID tag scan
    virtual void updateDisplay() = 0;                 // Draw your mission screen

    // OPTIONAL — override if needed (defaults provided):
    virtual String getCurrentDifficulty();   // Returns "None" — override to return "Easy"/"Medium"/"Hard"/"Extreme"
    virtual bool isComplete();               // Returns false — override to return true when mission can be completed
    virtual void setMissionStartMs(unsigned long);  // Receives the timer start time
    virtual int getVisitCount() const;               // For state persistence
    virtual String getSelectedLocationsString() const;  // For state persistence
    virtual std::vector<int> getCollectedResources() const;  // Resources collected

    // Safe Cracker integration (optional):
    virtual bool findResourceTag(String tag, int& outLocIdx, int& outResType);
    virtual void addResource(int locIdx, int resType);
};
```

**The 3 functions you MUST write:**
- `setup()` — Generates random locations, initializes your game state
- `processLocation(String tag)` — Called when a player scans an RFID tag during your mission
- `updateDisplay()` — Draws your mission's current state on the 240×240 screen

---

## Step 2: Create Your Header File

Create `YourMission.h` in the project root. Use `PushMission.h` as your template:

```cpp
#pragma once
#include <Arduino.h>
#include "MissionBase.h"
#include "Config.h"
#include "StateManager.h"
#include <vector>

class YourMission : public MissionBase {
public:
    explicit YourMission(StateManager& sm);

    // MissionBase overrides
    void   setup() override;
    void   processLocation(String locationTag) override;
    void   updateDisplay() override;
    String getCurrentDifficulty() override;
    bool   isComplete() override { return visitedCount >= 1; }
    void   setMissionStartMs(unsigned long t) override { missionStartMs = t; }
    int    getVisitCount() const override { return visitedCount; }

private:
    StateManager& stateManager;
    int visitedCount = 0;
    unsigned long missionStartMs = 0;

    // Add your mission-specific variables here
};
```

---

## Step 3: Create Your Source File

Create `YourMission.cpp`:

```cpp
#include "YourMission.h"
#include <M5Dial.h>

// These are defined in the main .ino file — you can call them
extern void playSuccessTone();
extern void playErrorTone();
extern bool devMode;
extern const char* LOCATION_NAMES[];
extern const char* RESOURCE_NAMES[];

YourMission::YourMission(StateManager& sm) : stateManager(sm) {}

void YourMission::setup() {
    visitedCount = 0;
    // Generate random locations, set up your game state
    // Use POI_LOCATIONS[] array (defined in Config.h) for location data
    // Use LOCATION_NAMES[] for display names
    updateDisplay();
}

void YourMission::processLocation(String locationTag) {
    // Match the scanned tag against POI_LOCATIONS
    for (int loc = 0; loc < TOTAL_LOCATIONS; loc++) {
        const LocationInfo& info = POI_LOCATIONS[loc];
        const String* tagSets[] = {
            info.weaponTags, info.securityTags,
            info.vehicleTags, info.moneyTags
        };
        for (int res = 0; res < RESOURCE_COUNT; res++) {
            for (int t = 0; t < 3; t++) {
                if (!tagSets[res][t].isEmpty() && locationTag == tagSets[res][t]) {
                    // Found a match! Do your game logic here
                    visitedCount++;
                    playSuccessTone();
                    updateDisplay();
                    return;
                }
            }
        }
    }
    // No match — optionally play error
    playErrorTone();
}

void YourMission::updateDisplay() {
    // Draw on the 240x240 round screen (center is 120,120)
    M5Dial.Lcd.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM);  // Center text alignment
    M5Dial.Display.setTextSize(2);
    M5Dial.Display.setTextColor(TFT_WHITE);
    M5Dial.Display.drawString("YOUR MISSION", 120, 80);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Visited: " + String(visitedCount), 120, 130);
}

String YourMission::getCurrentDifficulty() {
    // Return difficulty based on progress
    if (visitedCount >= 4) return "Extreme";
    if (visitedCount >= 3) return "Hard";
    if (visitedCount >= 2) return "Medium";
    if (visitedCount >= 1) return "Easy";
    return "None";
}
```

---

## Step 4: Add RFID Tags in Config.h

Open `Config.h` and add a tag array for your mission. Find the section with other mission tags (search for `MISSION_TAGS`) and add:

```cpp
// Your Mission tags
const String YOUR_MISSION_TAGS[] = {" XX XX XX XX"};  // Replace with real RFID tag hex
const int NUM_YOUR_MISSION_TAGS = 1;
```

**Tip:** Scan a new RFID card with the device in Admin Mode to see its tag hex printed to serial output.

---

## Step 5: Wire It Up in mission_M5_refactored.ino

Three small changes:

### 5a. Add the include (near top of file, with other mission includes)

```cpp
#include "YourMission.h"
```

### 5b. Add tag matching in processCardScan()

Find the `WAIT_FOR_MISSION_CARD` section (search for `// Push card` or `// Resource Hunt card`). Add your block before or after an existing mission:

```cpp
      // Your Mission card
      for (int i = 0; i < NUM_YOUR_MISSION_TAGS; i++) {
        if (cardUID == YOUR_MISSION_TAGS[i] && !YOUR_MISSION_TAGS[i].isEmpty()) {
          currentMissionCardUID = cardUID;
          displayMessage("YOUR\\nMISSION!", COLOR_INFO, 1500);
          startMission(new YourMission(stateManager), nullptr);
          logger.log("Your Mission started");
          return;
        }
      }
```

**Important:** Use `\\n` (backslash + n) in displayMessage strings for line breaks, NOT `\n`.

### 5c. (Optional) Configure before starting

If your mission needs setup parameters, use the lambda:

```cpp
          startMission(new YourMission(stateManager), [](MissionBase* m) {
            ((YourMission*)m)->setSomeOption(42);
          });
```

---

## Reference: Existing Missions

| Mission | Complexity | Good Template For |
|---------|-----------|-------------------|
| `PushMission` | Simple | Ordered step-by-step games |
| `FreeRoamMission` | Simple | Visit-any-location games |
| `ResourceHuntMission` | Medium | Collect-specific-items games |
| `StoryMission` | Complex | Story-driven narrative games |
| `SafeCrackerMission` | Complex | Mini-games with custom UI |

---

## Key Constants & Arrays You'll Use

| Name | Where | What |
|------|-------|------|
| `POI_LOCATIONS[]` | Config.h | Array of all 25 locations with RFID tags |
| `LOCATION_NAMES[]` | .ino file | Human-readable names for each location |
| `RESOURCE_NAMES[]` | .ino file | Names: "Weapons", "Security", "Vehicle", "Money" |
| `TOTAL_LOCATIONS` | Config.h | Number of locations (25) |
| `RESOURCE_COUNT` | Config.h | Number of resource types (4) |
| `COLOR_ERROR` | Config.h | Red color for errors |
| `COLOR_SUCCESS` | Config.h | Green color for success |
| `COLOR_WARNING` | Config.h | Orange/yellow for warnings |
| `COLOR_INFO` | Config.h | Blue for info messages |
| `DisplayText::*` | Config.h | Pre-defined UI text strings |

---

## Display Tips

- Screen is **240×240 pixels**, round. Center point is **(120, 120)**.
- Use `M5Dial.Display.setTextDatum(MC_DATUM)` for centered text.
- `setTextSize(1)` = small, `(2)` = medium, `(3)` = large, `(4)` = extra large.
- Colors: `TFT_WHITE`, `TFT_BLACK`, `TFT_RED`, `TFT_GREEN`, `TFT_YELLOW`, `TFT_CYAN`, `TFT_ORANGE`.
- Custom color: `M5Dial.Display.color565(R, G, B)` where R/G/B are 0–255.
- Always call `M5Dial.Lcd.fillScreen(TFT_BLACK)` first to clear the screen.

---

## Checklist

- [ ] Created `YourMission.h` with class declaration
- [ ] Created `YourMission.cpp` with `setup()`, `processLocation()`, `updateDisplay()`
- [ ] Implemented `getCurrentDifficulty()` returning "Easy"/"Medium"/"Hard"/"Extreme"
- [ ] Implemented `isComplete()` returning true when at least 1 location visited
- [ ] Added RFID tag array in `Config.h`
- [ ] Added `#include` in `mission_M5_refactored.ino`
- [ ] Added tag matching block in `processCardScan()`
- [ ] Tested: scan badge → scan mission card → plays your mission → scan completion card
