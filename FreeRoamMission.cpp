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
// Format: v4|C=count|S=loc0,loc1|N=name0~name1|V=0,1.
// v2/v3 states without N are also accepted and fall back to LOCATION_NAMES.

static String sanitizeMissionDisplayName(String name) {
    name.replace("|", " ");
    name.replace("~", " ");
    name.trim();
    return name;
}

static String sanitizeMissionHeaderName(String name) {
    name = sanitizeMissionDisplayName(name);
    int roundSuffix = name.indexOf(" - Round ");
    if (roundSuffix > 0) {
        name = name.substring(0, roundSuffix);
        name.trim();
    }
    return name;
}

String FreeRoamMission::displayNameForSpot(int spotIndex) const {
    if (spotIndex >= 0 && spotIndex < REQUIRED_LOCATIONS && missionDisplayNames[spotIndex][0] != '\0') {
        return String(missionDisplayNames[spotIndex]);
    }
    int loc = (spotIndex >= 0 && spotIndex < REQUIRED_LOCATIONS) ? missionLocations[spotIndex] : 0;
    if (loc < 0 || loc >= TOTAL_LOCATIONS) loc = 0;
    return String(LOCATION_NAMES[loc]);
}

void FreeRoamMission::setDisplayNameForSpot(int spotIndex, const String& name) {
    if (spotIndex < 0 || spotIndex >= REQUIRED_LOCATIONS) return;
    String clean = sanitizeMissionDisplayName(name);
    if (clean.length() == 0) {
        int loc = missionLocations[spotIndex];
        if (loc < 0 || loc >= TOTAL_LOCATIONS) loc = 0;
        clean = String(LOCATION_NAMES[loc]);
    }
    clean.toCharArray(missionDisplayNames[spotIndex], MISSION_DISPLAY_NAME_MAX);
}

String FreeRoamMission::getSelectedLocationsString() const {
    const int count = constrain(activeSpotCount, 1, REQUIRED_LOCATIONS);
    String s = "v4|C=" + String(count) + "|S=";
    for (int i = 0; i < count; i++) {
        if (i > 0) s += ",";
        s += String(missionLocations[i]);
    }
    s += "|N=";
    for (int i = 0; i < count; i++) {
        if (i > 0) s += "~";
        s += sanitizeMissionDisplayName(displayNameForSpot(i));
    }
    s += "|V=";
    for (int i = 0; i < count; i++) {
        if (i > 0) s += ",";
        s += missionSpotVisited[i] ? "1" : "0";
    }
    return s;
}

bool FreeRoamMission::parseSavedMissionState(const String& saved) {
    bool hasNames = saved.startsWith("v4|C=");
    bool hasCount = hasNames || saved.startsWith("v3|C=");
    bool legacy = saved.startsWith("v2|S=");
    if (!hasCount && !legacy) return false;

    int count = REQUIRED_LOCATIONS;
    int selStart = 5;
    if (hasCount) {
        int countEnd = saved.indexOf("|S=");
        if (countEnd < 0) return false;
        count = saved.substring(5, countEnd).toInt();
        if (count < 1 || count > REQUIRED_LOCATIONS) return false;
        selStart = countEnd + 3;
    }

    int namesSplit = hasNames ? saved.indexOf("|N=", selStart) : -1;
    int split = saved.indexOf("|V=", hasNames ? namesSplit + 3 : selStart);
    if (split < 0) return false;
    if (hasNames && namesSplit < 0) return false;

    String sel = hasNames ? saved.substring(selStart, namesSplit) : saved.substring(selStart, split);
    String names = hasNames ? saved.substring(namesSplit + 3, split) : "";
    String vis = saved.substring(split + 3);

    int parsedLocs[REQUIRED_LOCATIONS];
    bool parsedVisited[REQUIRED_LOCATIONS];
    String parsedNames[REQUIRED_LOCATIONS];
    int locCount = 0;
    int visCount = 0;
    int nameCount = 0;

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

    if (hasNames) {
        idx = 0;
        while (idx <= (int)names.length() && nameCount < REQUIRED_LOCATIONS) {
            int sep = names.indexOf('~', idx);
            String tok = (sep == -1) ? names.substring(idx) : names.substring(idx, sep);
            tok = sanitizeMissionDisplayName(tok);
            parsedNames[nameCount++] = tok;
            if (sep == -1) break;
            idx = sep + 1;
        }
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

    if (legacy) count = locCount;
    if (count < 1 || count > REQUIRED_LOCATIONS || locCount != count || visCount != count) return false;

    activeSpotCount = count;
    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionLocations[i] = i < count ? parsedLocs[i] : 0;
        missionSpotVisited[i] = i < count ? parsedVisited[i] : false;
        missionDisplayNames[i][0] = '\0';
        if (i < count) {
            if (hasNames && i < nameCount && parsedNames[i].length() > 0) {
                setDisplayNameForSpot(i, parsedNames[i]);
            } else {
                setDisplayNameForSpot(i, String(LOCATION_NAMES[missionLocations[i]]));
            }
        }
    }
    return true;
}
void FreeRoamMission::buildMissionSelection() {
    activeSpotCount = REQUIRED_LOCATIONS;
    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionSpotVisited[i] = false;
    }

    if (devMode) {
        // Dev mode forces all 4 spots to GURU_HOME.
        for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
            missionLocations[i] = TOTAL_LOCATIONS - 1;
            setDisplayNameForSpot(i, String(LOCATION_NAMES[missionLocations[i]]));
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
        setDisplayNameForSpot(i, String(LOCATION_NAMES[missionLocations[i]]));
    }
}

// ---- setup ----

void FreeRoamMission::setup() {
    for (int i = 0; i < TOTAL_LOCATIONS; i++) poiVisited[i] = false;
    visitedCount = 0;
    browseIndex = 0;

    for (int i = 0; i < REQUIRED_LOCATIONS; i++) {
        missionLocations[i] = 0;
        missionDisplayNames[i][0] = '\0';
        missionSpotVisited[i] = false;
    }

    String saved = stateManager.getSelectedLocations();
    bool restored = parseSavedMissionState(saved);
    if (!restored) {
        buildMissionSelection();
    }

    for (int i = 0; i < activeSpotCount; i++) {
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
        Serial.println("[ROAM] Created new " + String(activeSpotCount) + "-spot mission");
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

        for (int i = 0; i < activeSpotCount; i++) {
            if (missionLocations[i] == locIdx) {
                spotIdx = i;
                break;
            }
        }
        if (spotIdx < 0) {
            Serial.println("[ROAM] Tag is valid POI but not one of this mission's active spots");
            return;
        }
    }

    if (missionSpotVisited[spotIdx]) {
        Serial.println("[ROAM] POI already visited: " + displayNameForSpot(spotIdx));
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

    Serial.println("[ROAM] Visited " + displayNameForSpot(spotIdx) +
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
    const int total = constrain(activeSpotCount, 1, REQUIRED_LOCATIONS);
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
    const bool vaultTheme = ACTIVE_UI_THEME == THEME_VAULT;
    const uint16_t vaultBg = M5Dial.Display.color565(28, 12, 0);
    const uint16_t vaultPanel = M5Dial.Display.color565(44, 20, 0);
    const uint16_t vaultAccent = M5Dial.Display.color565(255, 150, 40);
    const uint16_t vaultAccentHi = M5Dial.Display.color565(255, 190, 70);
    const uint16_t vaultWarn = M5Dial.Display.color565(255, 90, 40);

    // ---- Handle encoder scroll (before dirty check) ----
    int enc   = M5Dial.Encoder.read();
    int delta = enc - lastEncoder;
    bool encoderMoved = false;
    if (abs(delta) >= 4) {
        int dir = (delta > 0) ? 1 : -1;
        const int count = constrain(activeSpotCount, 1, REQUIRED_LOCATIONS);
        browseIndex = (browseIndex + dir + count) % count;
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

    const int count = constrain(activeSpotCount, 1, REQUIRED_LOCATIONS);
    browseIndex = constrain(browseIndex, 0, count - 1);
    bool isVisited = missionSpotVisited[browseIndex];
    String poiName  = displayNameForSpot(browseIndex);
    String missionHeader = sanitizeMissionHeaderName(stateManager.getActiveMissionName());

    M5Dial.Lcd.fillScreen(vaultTheme ? vaultBg : TFT_BLACK);
    M5Dial.Display.setTextDatum(MC_DATUM);
    if (vaultTheme) {
        // Keep scanlines away from screen edges to avoid visible top/bottom bars.
        for (int y = 2; y < 238; y += 4) {
            M5Dial.Display.drawFastHLine(0, y, 240, vaultPanel);
        }
    }

    // ---- Always-on mission timer (top) ----
    if (missionStartMs > 0) {
        unsigned long elapsed   = millis() - missionStartMs;
        unsigned long remaining = (elapsed >= MISSION_TIMEOUT_MS) ? 0UL : (MISSION_TIMEOUT_MS - elapsed);
        unsigned long mins = remaining / 60000UL;
        unsigned long secs = (remaining / 1000UL) % 60UL;
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%lu:%02lu", mins, secs);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.setTextColor(vaultTheme ? (remaining < 60000UL ? vaultWarn : vaultAccentHi) : (remaining < 60000UL ? TFT_RED : TFT_YELLOW));
        M5Dial.Display.drawString(String(tbuf), cx, 15);
        M5Dial.Display.setTextSize(1);  // restore
    }

    // ---- Header (below timer) ----
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(vaultTheme ? vaultAccentHi : TFT_CYAN);
    M5Dial.Display.drawString(vaultTheme ? String("VAULT MISSION  ") + String(visitedCount) + "/" + String(count)
                                         : String("MISSION PROGRESS  ") + String(visitedCount) + "/" + String(count),
                             cx, 38);

    // ---- Progress pips ----
    int pipsY = vaultTheme ? (M5Dial.Display.height() - 30) : (M5Dial.Display.height() - 16);
    drawProgressPips(cx, pipsY, visitedCount, browseIndex);

    const int maxW  = 190;
    const int nameCY = 123;

    if (isVisited) {
        M5Dial.Display.setTextSize(3);
        // Thin outline pass for cleaner title readability on the dial.
        M5Dial.Display.setTextColor(vaultTheme ? vaultPanel : TFT_BLACK);
        M5Dial.Display.drawString("VISITED", cx - 1, 72);
        M5Dial.Display.drawString("VISITED", cx + 1, 72);
        M5Dial.Display.drawString("VISITED", cx, 71);
        M5Dial.Display.drawString("VISITED", cx, 73);
        M5Dial.Display.setTextColor(vaultTheme ? vaultAccentHi : TFT_GREEN);
        M5Dial.Display.drawString("VISITED", cx, 72);

        M5Dial.Display.drawLine(40, 93, 200, 93, vaultTheme ? vaultPanel : TFT_DARKGREY);

        M5Dial.Display.setTextColor(vaultTheme ? vaultAccentHi : TFT_GREEN);
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
        M5Dial.Display.setTextColor(vaultTheme ? vaultAccent : TFT_DARKGREY);
        String navV = "< " + String(browseIndex + 1) + "/" + String(count) + " >";
        M5Dial.Display.drawString(navV, cx, M5Dial.Display.height() - 50);

        M5Dial.Display.setTextColor(vaultTheme ? vaultAccentHi : TFT_GREEN);
        M5Dial.Display.drawString("[ VISITED ]", cx, M5Dial.Display.height() - 33);

    } else {
        String headerLabel = missionHeader.length() > 0 ? missionHeader : String("SCAN");
        int headerSize = headerLabel.length() > 9 ? 2 : 3;
        M5Dial.Display.setTextSize(headerSize);
        while (headerSize > 1 && M5Dial.Display.textWidth(headerLabel) > maxW) {
            headerSize--;
            M5Dial.Display.setTextSize(headerSize);
        }
        // Thin 1px border around mission title for legibility.
        M5Dial.Display.setTextColor(vaultTheme ? vaultPanel : TFT_BLACK);
        M5Dial.Display.drawString(headerLabel, cx - 1, 72);
        M5Dial.Display.drawString(headerLabel, cx + 1, 72);
        M5Dial.Display.drawString(headerLabel, cx, 71);
        M5Dial.Display.drawString(headerLabel, cx, 73);
        M5Dial.Display.setTextColor(vaultTheme ? TFT_WHITE : TFT_ORANGE);
        M5Dial.Display.drawString(headerLabel, cx, 72);

        M5Dial.Display.drawLine(40, 93, 200, 93, vaultTheme ? vaultPanel : TFT_DARKGREY);

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
        M5Dial.Display.setTextColor(vaultTheme ? vaultAccent : TFT_DARKGREY);
        String navV = "< " + String(browseIndex + 1) + "/" + String(count) + " >";
        M5Dial.Display.drawString(navV, cx, M5Dial.Display.height() - 50);
    }

    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(vaultTheme ? vaultAccentHi : TFT_YELLOW);
    M5Dial.Display.drawString("Reward: " + getCurrentDifficulty(), cx, 53);
    if (vaultTheme) {
        // Keep this helper above the bezel edge and high-contrast against scanlines.
        M5Dial.Display.setTextColor(TFT_WHITE);
        M5Dial.Display.setTextSize(2);
        M5Dial.Display.drawString("HOLD 3S: CONTROL", cx, 224);
        M5Dial.Display.setTextSize(1);
    }
}
