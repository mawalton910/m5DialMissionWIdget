#include "FreeRoamMission.h"
#include <M5Dial.h>
#include <algorithm>

// Defined in mission_M5.ino
extern void playSuccessTone();
extern void playErrorTone();
extern bool devMode;
extern const char* LOCATION_NAMES[];

// ---- Construction ----

FreeRoamMission::FreeRoamMission(StateManager& sm)
    : stateManager(sm) {}

// ---- getSelectedLocationsString ----
// Format: v2|S=loc0,loc1,loc2,loc3|V=0,1,0,1

String FreeRoamMission::getSelectedLocationsString() const {
    String s = "v2|S=";
    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        if (i > 0) s += ",";
        s += String(missionLocations[i]);
    }
    s += "|V=";
    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        if (i > 0) s += ",";
        s += missionSpotVisited[i] ? "1" : "0";
    }
    return s;
}

bool FreeRoamMission::parseSavedMissionState(const String& saved) {
    if (!saved.startsWith("v2|S=")) return false;

    int split = saved.indexOf("|V=");
    if (split < 0) return false;

    String sel = saved.substring(5, split);
    String vis = saved.substring(split + 3);

    int parsedLocs[REQUIRED_LOCATIONS];
    bool parsedVisited[REQUIRED_LOCATIONS];
    int locCount = 0;
    int visCount = 0;

    int idx = 0;
    while (idx <= (int)sel.length() && locCount < REQUIRED_LOCATIONS) {
        int comma = sel.indexOf(',', idx);
        String tok = (comma == -1) ? sel.substring(idx) : sel.substring(idx, comma);
        tok.trim();
        if (tok.length() > 0) {
            int loc = tok.toInt();
            if (loc < 0 || loc >= TOTAL_LOCATIONS) return false;
            parsedLocs[locCount++] = loc;
        }
        if (comma == -1) break;
        idx = comma + 1;
    }

    idx = 0;
    while (idx <= (int)vis.length() && visCount < REQUIRED_LOCATIONS) {
        int comma = vis.indexOf(',', idx);
        String tok = (comma == -1) ? vis.substring(idx) : vis.substring(idx, comma);
        tok.trim();
        if (tok == "0" || tok == "1") {
            parsedVisited[visCount++] = (tok == "1");
        } else {
            return false;
        }
        if (comma == -1) break;
        idx = comma + 1;
    }

    if (locCount != REQUIRED_LOCATIONS || visCount != REQUIRED_LOCATIONS) return false;

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionLocations[i] = parsedLocs[i];
        missionSpotVisited[i] = parsedVisited[i];
    }
    return true;
}

void FreeRoamMission::buildMissionSelection() {
    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionSpotVisited[i] = false;
    }

    if (devMode) {
        // Dev mode forces all 4 spots to GURU_HOME.
        for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
            missionLocations[i] = TOTAL_LOCATIONS - 1;
        }
        return;
    }

    // Production mode: pick 4 unique random POIs from non-dev locations.
    const int prodCount = max(1, TOTAL_LOCATIONS - 1);
    int pool[TOTAL_LOCATIONS];
    for (int i = 0; i < prodCount; i++) pool[i] = i;

    for (int i = prodCount - 1; i > 0; i--) {
        int j = random(i + 1);
        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionLocations[i] = pool[i % prodCount];
    }
}

// ---- setup ----

void FreeRoamMission::setup() {
    for (int i = 0; i < TOTAL_LOCATIONS; i++) poiVisited[i] = false;
    visitedCount = 0;
    browseIndex = 0;

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionLocations[i] = 0;
        missionSpotVisited[i] = false;
    }

    String saved = stateManager.getSelectedLocations();
    bool restored = parseSavedMissionState(saved);
    if (!restored) {
        buildMissionSelection();
    }

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        if (missionSpotVisited[i]) {
            int loc = missionLocations[i];
            if (loc >= 0 && loc < TOTAL_LOCATIONS) {
                if (!poiVisited[loc]) poiVisited[loc] = true;
                visitedCount++;
                browseIndex = i;
            }
        }
    }

    if (restored) {
        Serial.println("[ROAM] Restored mission state with " + String(visitedCount) + " completed spots");
    } else {
        Serial.println("[ROAM] Created new 4-spot mission");
    }

    stateManager.setSelectedLocations(getSelectedLocationsString());
    stateManager.setVisitedCount(visitedCount);

    lastEncoder = M5Dial.Encoder.read();
    updateDisplay();
}

int FreeRoamMission::findDevSpotForTag(const String& locationTag) const {
    const LocationInfo& dev = POI_LOCATIONS[TOTAL_LOCATIONS - 1];

    // Use exactly 4 dev-mode tags: prefer slot 1 (new tags), fallback to slot 0.
    const String devTags[REQUIRED_LOCATIONS] = {
        !dev.weaponTags[1].isEmpty() ? dev.weaponTags[1] : dev.weaponTags[0],
        !dev.securityTags[1].isEmpty() ? dev.securityTags[1] : dev.securityTags[0],
        !dev.vehicleTags[1].isEmpty() ? dev.vehicleTags[1] : dev.vehicleTags[0],
        !dev.moneyTags[1].isEmpty() ? dev.moneyTags[1] : dev.moneyTags[0]
    };

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        if (!devTags[i].isEmpty() && locationTag == devTags[i]) {
            return i;
        }
    }
    return -1;
}

// ---- processLocation ----

void FreeRoamMission::processLocation(String locationTag) {
    if (locationTag.isEmpty()) return;
    int spotIdx = -1;
    int locIdx = -1;

    if (devMode) {
        spotIdx = findDevSpotForTag(locationTag);
        locIdx = TOTAL_LOCATIONS - 1;
        if (spotIdx < 0) {
            Serial.println("[ROAM] Dev mode ignoring non-GURU_HOME spot tag: " + locationTag);
            return;
        }
    } else {
        locIdx = findLocationForTag(locationTag);
        if (locIdx < 0) {
            Serial.println("[ROAM] Tag not in POI list, ignoring: " + locationTag);
            return;
        }

        for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
            if (missionLocations[i] == locIdx) {
                spotIdx = i;
                break;
            }
        }
        if (spotIdx < 0) {
            Serial.println("[ROAM] Tag is valid POI but not one of this mission's 4 spots");
            return;
        }
    }

    if (missionSpotVisited[spotIdx]) {
        Serial.println("[ROAM] POI already visited: " + String(LOCATION_NAMES[locIdx]));
        playErrorTone();
        M5Dial.Lcd.fillScreen(TFT_RED);
        M5Dial.Display.setTextDatum(MC_DATUM);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.setTextColor(TFT_WHITE);
        M5Dial.Display.drawString("ALREADY", 120, 90);
        M5Dial.Display.drawString("VISITED!", 120, 115);
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.setTextColor(TFT_YELLOW);
        M5Dial.Display.drawString("Try a different POI", 120, 152);
        delay(1600);
        updateDisplay();
        return;
    }

    missionSpotVisited[spotIdx] = true;
    poiVisited[locIdx] = true;
    visitedCount++;
    browseIndex = spotIdx;
    stateManager.setSelectedLocations(getSelectedLocationsString());
    stateManager.setVisitedCount(visitedCount);

    Serial.println("[ROAM] Visited " + String(LOCATION_NAMES[locIdx]) +
                   " (" + String(visitedCount) + " total)");
    playSuccessTone();
    updateDisplay();
}

// ---- getCurrentDifficulty ----

String FreeRoamMission::getCurrentDifficulty() {
    switch (visitedCount) {
        case 0:  return "None";
        case 1:  return "Easy";
        case 2:  return "Medium";
        case 3:  return "Hard";
        default: return "Extreme";
    }
}

// ---- drawProgressPips ----

void FreeRoamMission::drawProgressPips(int cx, int cy, int done, int activeIndex) {
    const int total = REQUIRED_LOCATIONS;
    const int PIP_R   = 6;
    const int SPACING = 18;
    int startX = cx - ((total - 1) * SPACING) / 2;
    for (int i = 0; i < total; i++) {
        int px = startX + i * SPACING;
        if (missionSpotVisited[i]) {
            M5Dial.Display.fillCircle(px, cy, PIP_R, TFT_GREEN);
        } else {
            M5Dial.Display.drawCircle(px, cy, PIP_R, TFT_DARKGREY);
        }

        if (i == activeIndex) {
            M5Dial.Display.drawCircle(px, cy, PIP_R + 2, TFT_ORANGE);
            M5Dial.Display.drawCircle(px, cy, PIP_R + 1, TFT_ORANGE);
        }
    }
}

int FreeRoamMission::findLocationForTag(const String& locationTag) const {
    for (int locIdx = 0; locIdx < TOTAL_LOCATIONS; locIdx++) {
        const LocationInfo& loc = POI_LOCATIONS[locIdx];
        const String* tagSets[RESOURCE_COUNT] = {
            loc.weaponTags, loc.securityTags, loc.vehicleTags, loc.moneyTags
        };
        for (int res = 0; res < RESOURCE_COUNT; res++) {
            for (int t = 0; t < 3; t++) {
                if (!tagSets[res][t].isEmpty() && locationTag == tagSets[res][t]) {
                    return locIdx;
                }
            }
        }
    }
    return -1;
}

// ---- updateDisplay ----

void FreeRoamMission::updateDisplay() {
    const int cx = M5Dial.Display.width() / 2;  // 120

    // ---- Handle encoder scroll (before dirty check) ----
    int enc   = M5Dial.Encoder.read();
    int delta = enc - lastEncoder;
    bool encoderMoved = false;
    if (abs(delta) >= 4) {
        int dir = (delta > 0) ? 1 : -1;
        browseIndex = (browseIndex + dir + REQUIRED_LOCATIONS) % REQUIRED_LOCATIONS;
        lastEncoder = enc;
        encoderMoved = true;
    }

    // ---- Timer dirty: redraw once per second while mission active ----
    unsigned long currentSec = (missionStartMs > 0) ? (millis() / 1000UL) : 0UL;
    static unsigned long lastTimerSec = 0xFFFFFFFFUL;
    bool timerDirty = (missionStartMs > 0) && (currentSec != lastTimerSec);

    // ---- Dirty-flag: skip redraw if nothing changed ----
    static int  lastDrawnBrowse  = -1;
    static int  lastDrawnVisited = -1;
    if (!encoderMoved && !timerDirty
            && browseIndex  == lastDrawnBrowse
            && visitedCount == lastDrawnVisited) {
        return;
    }
    lastDrawnBrowse  = browseIndex;
    lastDrawnVisited = visitedCount;
    if (timerDirty) lastTimerSec = currentSec;

    browseIndex = constrain(browseIndex, 0, REQUIRED_LOCATIONS - 1);
    int currentLocIdx = missionLocations[browseIndex];
    bool isVisited = missionSpotVisited[browseIndex];
    String poiName  = String(LOCATION_NAMES[currentLocIdx]);

    M5Dial.Lcd.fillScreen(TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM);

    // ---- Always-on mission timer (top) ----
    if (missionStartMs > 0) {
        unsigned long elapsed   = millis() - missionStartMs;
        unsigned long remaining = (elapsed >= MISSION_TIMEOUT_MS) ? 0UL : (MISSION_TIMEOUT_MS - elapsed);
        unsigned long mins = remaining / 60000UL;
        unsigned long secs = (remaining / 1000UL) % 60UL;
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%lu:%02lu", mins, secs);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.setTextColor(remaining < 60000UL ? TFT_RED : TFT_YELLOW);
        M5Dial.Display.drawString(String(tbuf), cx, 15);
        M5Dial.Display.setTextSize(1);  // restore
    }

    // ---- Header (below timer) ----
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(TFT_CYAN);
    M5Dial.Display.drawString("MISSION PROGRESS  " + String(visitedCount) + "/" + String(REQUIRED_LOCATIONS), cx, 38);

    // ---- Progress pips ----
    drawProgressPips(cx, M5Dial.Display.height() - 16, visitedCount, browseIndex);

    const int maxW  = 190;
    const int nameCY = 123;

    if (isVisited) {
        M5Dial.Display.setTextSize(3);
        for (int ox = -1; ox <= 1; ox++)
            for (int oy = -1; oy <= 1; oy++)
                if (ox != 0 || oy != 0) {
                    M5Dial.Display.setTextColor(TFT_WHITE);
                    M5Dial.Display.drawString("VISITED", cx + ox, 72 + oy);
                }
        M5Dial.Display.setTextColor(TFT_GREEN);
        M5Dial.Display.drawString("VISITED", cx, 72);

        M5Dial.Display.drawLine(40, 93, 200, 93, TFT_DARKGREY);

        M5Dial.Display.setTextColor(TFT_GREEN);
        if ((int)poiName.length() > 15) {
            int splitPos = 15;
            for (int k = 14; k >= 0; k--) { if (poiName[k] == ' ') { splitPos = k; break; } }
            M5Dial.Display.setTextSize(2);
            M5Dial.Display.drawString(poiName.substring(0, splitPos), cx, nameCY - 10);
            M5Dial.Display.drawString(poiName.substring(splitPos + 1), cx, nameCY + 12);
        } else {
            int sz = 4; M5Dial.Display.setTextSize(sz);
            while (sz > 1 && M5Dial.Display.textWidth(poiName) > maxW) { sz--; M5Dial.Display.setTextSize(sz); }
            M5Dial.Display.drawString(poiName, cx, nameCY);
        }

        M5Dial.Display.setTextSize(1);
        M5Dial.Display.setTextColor(TFT_DARKGREY);
        String navV = "< " + String(browseIndex + 1) + "/" + String(REQUIRED_LOCATIONS) + " >";
        M5Dial.Display.drawString(navV, cx, M5Dial.Display.height() - 50);

        M5Dial.Display.setTextColor(TFT_GREEN);
        M5Dial.Display.drawString("[ VISITED ]", cx, M5Dial.Display.height() - 33);

    } else {
        M5Dial.Display.setTextSize(3);
        for (int ox = -1; ox <= 1; ox++)
            for (int oy = -1; oy <= 1; oy++)
                if (ox != 0 || oy != 0) {
                    M5Dial.Display.setTextColor(TFT_WHITE);
                    M5Dial.Display.drawString("SCAN", cx + ox, 72 + oy);
                }
        M5Dial.Display.setTextColor(TFT_ORANGE);
        M5Dial.Display.drawString("SCAN", cx, 72);

        M5Dial.Display.drawLine(40, 93, 200, 93, TFT_DARKGREY);

        M5Dial.Display.setTextColor(TFT_WHITE);
        if ((int)poiName.length() > 15) {
            int splitPos = 15;
            for (int k = 14; k >= 0; k--) { if (poiName[k] == ' ') { splitPos = k; break; } }
            M5Dial.Display.setTextSize(2);
            M5Dial.Display.drawString(poiName.substring(0, splitPos), cx, nameCY - 10);
            M5Dial.Display.drawString(poiName.substring(splitPos + 1), cx, nameCY + 12);
        } else {
            int sz = 3; M5Dial.Display.setTextSize(sz);
            while (sz > 1 && M5Dial.Display.textWidth(poiName) > maxW) { sz--; M5Dial.Display.setTextSize(sz); }
            M5Dial.Display.drawString(poiName, cx, nameCY);
        }

        M5Dial.Display.setTextSize(1);
        M5Dial.Display.setTextColor(TFT_DARKGREY);
        String navV = "< " + String(browseIndex + 1) + "/" + String(REQUIRED_LOCATIONS) + " >";
        M5Dial.Display.drawString(navV, cx, M5Dial.Display.height() - 50);

        M5Dial.Display.setTextColor(TFT_ORANGE);
        M5Dial.Display.drawString("[ ANY TAG AT THIS POI ]", cx, M5Dial.Display.height() - 33);
    }

    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(TFT_YELLOW);
    M5Dial.Display.drawString("Reward: " + getCurrentDifficulty(), cx, 53);
}
