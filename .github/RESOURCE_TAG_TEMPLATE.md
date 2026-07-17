# Resource Tag Configuration Guide

## ✅ IMPLEMENTATION COMPLETE!

The resource-based mission system is fully implemented and ready for production use!

## System Overview

Each location has **4 unique RFID tags** (1 per resource type):
- **WEAPONS** (displayed in RED in story text)
- **SECURITY** (displayed in YELLOW)  
- **VEHICLE** (displayed in CYAN)
- **MONEY** (displayed in GREEN)

Missions randomly select 4 locations and assign one resource to each. Players must scan **the specific resource tag** at that location to progress.

## Location Configuration

### Production Locations (24 total)
Alphabetically sorted from Belle Isle Outpost through West Village. Each has 4 unique resource tags configured in `Config.h`.

### Development Location: GURU_HOME (Index 24)
**Purpose**: Testing and development without traveling to production locations

**Location**: Last entry in `POI_LOCATIONS` array  
**Current Tags** (replace with your actual dev tags):
```cpp
{ "GURU_HOME",
  " FF 38 DC 64",    // WEAPONS (dev tag)
  " 53 FC 04 03",    // SECURITY (dev tag)
  " F3 21 E9 02",    // VEHICLE (dev tag)
  " EF B2 D7 64" }   // MONEY (dev tag)
```

**Activation**:
1. Enter admin mode (scan admin badge)
2. Navigate to \"Toggle Dev Mode\" (option 3)
3. Press button - status shows \"DEV MODE: ON\" (green)
4. Exit admin mode
5. Start new mission - all 4 objectives will use GURU_HOME
6. Scan your 4 dev tags at your workstation to complete mission

**Benefits**:
- Test full mission flow without leaving your desk
- Verify resource scanning logic
- Debug story generation and timing
- Keep dev tags separate from production tags

## Story Format Examples

Stories are dynamically generated with category-specific action verbs and color-coded resource names:

**Heist Category**:
- "Steal the **WEAPONS** at Renaissance Hub to prepare for the big score."
- "Secure the **SECURITY** at Guardian Plaza to keep the score clean and quiet."
- "Extract the **VEHICLE** from Iron Depot to move the take before heat spikes."
- "Acquire the **MONEY** at Belle Isle Outpost to clear the exit for a fast pull."

**Police Category**:
- "Confiscate the **WEAPONS** at TechTown for the war on crime."
- "Seize the **SECURITY** at Spirits Watch to protect and serve the city."
- "Impound the **VEHICLE** from Campus Martius to uphold the law and order."
- "Secure the **MONEY** at Packard Ruins to stop illegal operations."

**Other Categories**: Hacking, Job, Vehicle, Drug, Medical, Community - each with 4 unique action verbs

### Resource Highlighting
Resource names appear in **color** in the story text:
- WEAPONS = Red (RGB565: 0xF800)
- SECURITY = Yellow (RGB565: 0xFFE0)
- VEHICLE = Cyan (RGB565: 0x07FF)
- MONEY = Green (RGB565: 0x07E0)

Location names also appear in color (cyan) when showing visit status.

## Tag Configuration in Config.h

All 24 production locations + GURU_HOME are configured in the `POI_LOCATIONS` array.  
**Note**: Location names reference the `LOCATION_NAMES[]` array defined in .ino file.

```cpp
const LocationInfo POI_LOCATIONS[] = {
    { LOCATION_NAMES[0],               // Belle Isle Outpost (from .ino)
      " DF 6D E0 64",              // WEAPONS
      " 52 D3 BB 29",              // SECURITY
      " 1D BE 2E 03 81 00 00",     // VEHICLE
      " 1D 6C A1 02 81 00 00" },   // MONEY
    // ... 22 more production locations ...
    { LOCATION_NAMES[23],              // West Village (from .ino)
      " FF BC D4 64",              // WEAPONS
      " 55 64 82 9F",              // SECURITY
      " F7 29 81 9F",              // VEHICLE
      " B3 4C 81 9F" },            // MONEY
    // === DEV LOCATION ===
    { LOCATION_NAMES[24],              // Guru Home (from .ino)Workflow

### Production Testing (All 24 Locations)
1. **Compile & Upload**: Flash code to M5Dial device
2. **Scan badge**: Start player identification
3. **Scan mission card**: Select category (Heist, Police, etc.)
4. **Read story pages**: Note which resources at which locations
5. **Travel to locations**: Visit the 4 specified real-world locations
6. **Scan correct tags**: Scan the specific resource tag shown in story
7. **Monitor progress**: Serial output shows `Location visited: Renaissance Hub (WEAPONS)` and `Progress: 1/4`
8. **Complete mission**: Return and scan completion card

### Development Testing (GURU_HOME Only)
1. **Enter admin mode**: Scan admin badge
2. **Enable dev mode**: Navigate to option 3, press button (shows green \"DEV MODE: ON\")
3. **Exit admin mode**: Scan admin badge again
4. **Start mission**: Scan any badge + mission card
5. **Verify GURU_HOME**: All 4 story pages should show GURU_HOME location
6. **Scan dev tags**: Scan all 4 resource tags at your workstation
   - Serial output: `DEV MODE: Using GURU_HOME for all locations`
7. **Complete**: Scan completion card
8. **Disable dev mode**: Re-enter admin mode, toggle off (shows red \"DEV MODE: OFF\")

### Admin Tag Identification
1. **Enter admin mode**: Scan admin badge
2. **Select option 1**: Turn dial, press button when "Scan Tag Info" selected
3. **Scan unknown tag**: Device identifies tag type, location, resource
4. **Example output**: "Location: Renaissance Hub" / "Resource: WEAPONS" / "Type: Mission Card (HEIST)"
5. **Exit**: Scan admin badge

## Updating Tags

### Changing Location Names (EASY!)
All location names are centralized in `POIStoryDualSendNoSoundv4.ino` for easy editing:

```cpp
// Lines 31-56 in .ino file - Just edit the names here!
const char* LOCATION_NAMES[] = {
    "Belle Isle Outpost",     // 0
    "Brush Park",             // 1
    "Campus Martius",         // 2
    // ... edit any name ...
    "GURU_HOME"               // 24 - DEV LOCATION
};
```

**To rename a location:**
1. Find the name in the `LOCATION_NAMES[]` array (lines 210-234)
2. Change the text in quotes
3. Keep the same array index position
4. Done! The name updates everywhere automatically

**Example:** Change "Belle Isle Outpost" to "Mystery Island"
```cpp
const char* LOCATION_NAMES[] = {
    "Mystery Island",  // Changed from "Belle Isle Outpost"
    "Brush Park",      // Keep other names the same
    // ...
};
```
### Replacing GURU_HOME Dev Tags
Edit line 337-342 in `Config.h`:
```cpp
{ "GURU_HOME",
  " YOUR TAG 01",     // Replace with your WEAPONS tag
  " YOUR TAG 02",     // Replace with your SECURITY tag
  " YOUR TAG 03",     // Replace with your VEHICLE tag
  " YOUR TAG 04" }    // Replace with your MONEY tag
```

###System Changes from Original Design

### What Changed
- **Original**: Each location had 4 duplicate tags (for redundancy)
- **Current**: Each location has 4 unique tags (one per resource type)
- **Assignment**: System randomly picks which resource to target at each of 4 locations per mission
- **Validation**: Players must scan the **specific resource tag** to progress

### Why Resource-Based?
- **Dynamic gameplay**: Same location can have different objectives across missions
- **Story integration**: Resource names embedded in narrative (GTA-style supply gathering)
- **Visual feedback**: Color-coded resource names help players identify objectives
- **Scalability**: Easy to add new locations without complex story rewrites

### Development Benefits
- **Guru Home**: Test at desk without traveling to 24 production locations
- **Admin mode**: Identify tags, control props, toggle dev mode in field
- **Logging**: SPIFFS-based persistent log with timestamps for debugging; WiFi connections logged with IP address
- **State restore**: Badge-reset allows player swaps without losing mission progress
- **WiFi diagnostics**: Connection success/failure logged to serial monitor and mission log file

---

## Quick Reference

**Total Locations**: 25 (24 production + 1 dev)  
**Tags per Location**: 4 (WEAPONS, SECURITY, VEHICLE, MONEY)  
**Missions per Session**: 1 at a time, 4 random locations  
**Timer**: 45 minutes default (configurable)  
**Admin Badge Tags**: Configured in `ADMIN_BADGE_TAGS` array  
**Dev Location**: GURU_HOME (index 24, toggle via admin mode)

**Color Codes**:
- RED (0xF800) = WEAPONS resource text
- YELLOW (0xFFE0) = SECURITY resource text
- CYAN (0x07FF) = VEHICLE resource text, location names
- GREEN (0x07E0) = MONEY resource text, success screens

---

**Ready for production!** System fully tested with resource scanning, admin features, and GURU_HOME dev mode
### Remapping Existing Location
Find the location in `POI_LOCATIONS` and update its 4 tags:
```cpp
{ "Renaissance Hub",
  " 6F 3B D5 64",              // Keep or replace WEAPONS
  " 3F D6 CD 64",              // Keep or replace SECURITY
  " 1D 7F 3A 03 81 00 00",     // Keep or replace VEHICLE
  " 1D A3 99 03 81 00 00" },   // Keep or replace MONEY
```

## Admin Mode Features

### Tag Identification
Use admin mode option 1 to identify any unknown tag:
- Shows tag type (location, mission, control card)
- For location tags: displays location name and resource type
- For mission cards: shows category
- Useful for verifying tag configuration and troubleshooting

### Prop Lock/Unlock (Option 2)
HTTP endpoints for prop management:
- **Lock**: POST to `http://iot.rke.world/iot/lockProp`
- **Unlock**: POST to `http://iot.rke.world/iot/unlockProp`
- Payload: `{\"uuid\":\"badge_uid\",\"deviceSerial\":\"device_serial\"}`
- Useful for field testing and remote prop control

### Dev Mode Toggle (Option 3)
- **Enable**: Forces all missions to use GURU_HOME location
- **Disable**: Returns to normal random location selection
- Status visible in admin menu with color indicator
- Serial log shows: `DEV MODE: Using GURU_HOME for all locations` when enabled

## Key Implementation Details

### Resource Assignment Logic
```cpp
// Ensures one of each resource type per mission
std::vector<int> shuffledResources = {0, 1, 2, 3};  // WEAPONS, SECURITY, VEHICLE, MONEY
std::shuffle(shuffledResources.begin(), shuffledResources.end(), rng);
// Assigns shuffled resources to 4 selected locations
```

### Tag Validation
```cpp
// Run at startup - checks all location tags
bool validateTags() {
    // Verifies format: leading space, correct length (6-25 chars)
    // Logs errors with location name and resource type
    return allValid;
}hat resource at that location
7. **Check serial output**: Should show:
   ```
   Location visited: Renaissance Hub (WEAPONS)
   Progress: 1/4
   ```

## Remapping Tags (Optional)

If you want different physical tags for each resource type, update [Config.h](Config.h):

```cpp
{ "Your Location",
  " AA BB CC DD",     // Replace with your WEAPONS tag
  " EE FF 00 11",     // Replace with your SECURITY tag
  " 22 33 44 55",     // Replace with your VEHICLE tag
  " 66 77 88 99" },   // Replace with your MONEY tag
```

## What Changed from Original Design

- Each location originally had 4 **duplicate** tags (for redundancy)
- Now each location has 4 **unique** tags (one per resource type)
- System randomly picks which resource to target at each location per mission
- Players must scan the **specific resource tag** to complete that location

---

**Ready to test!** Upload the code and start a mission to see the resource system in action.
