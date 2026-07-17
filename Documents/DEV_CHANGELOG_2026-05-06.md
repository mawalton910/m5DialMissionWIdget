# Dev Change Log - 2026-05-06

## Scope
This log captures firmware and API updates made during migration to a new game context.

## Repositories Touched
- Firmware: i:\Scenario Writing\MW5\Secondary\mission_M5_refactored
- API: i:\Scenario Writing\Creator Links\api-iot-main

## 1) Endpoint in Question
Primary endpoint used by the widget completion flow:
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\secrets.h:55
  - MISSION_API_ENDPOINT = https://iot.rke.world/iot/secondaryMissionComplete

Server-side handler for that endpoint:
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1072
  - router.post('/secondaryMissionComplete', ...)
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1136
  - router.post('/secondaryMissionCompleteMulti', ...)

## 2) Firmware Changes

### 2.1 Mission timer reduced to 15 minutes
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:49
  - const int MISSION_TIMEOUT_MIN = 15;
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:50
  - MISSION_TIMEOUT_MS derived from MISSION_TIMEOUT_MIN

### 2.2 Completion payload now includes game_id
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:1391
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:1392
  - Added game_id field in completion POST JSON

Before:

    {
      "mac_address": "34:B7:DA:56:0E:D8",
      "serial_number": "ggdevtester1",
      "difficulty": "Easy",
      "uuid": "8F3ABB1F",
      "uuids": []
    }

After:

    {
      "mac_address": "34:B7:DA:56:0E:D8",
      "serial_number": "ggdevtester1",
      "game_id": "69ae08e1e0e9cdc11c2548af",
      "difficulty": "Easy",
      "uuid": "8F3ABB1F",
      "uuids": []
    }

### 2.3 New game + network config references
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\secrets.h:69
  - DEVICE_GAME_ID = 69ae08e1e0e9cdc11c2548af
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\secrets.h:22
  - WIFI_STATIC_IP_ENABLED = 0 (DHCP)

### 2.4 Free Roam auto-start and UX updates
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:277
  - handleConfirmPlayersTouch(x, y) enabled touch on confirm screen
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:1211
  - drawMissionStartingScreen() GTA-style splash
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:1622
  - startFreeRoamFlow(showSplash)
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:269, 382, 556, 569, 584
  - state transitions now route into startFreeRoamFlow

### 2.5 Logging controls
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:39
  - SERIAL_LOG_LEVEL = SERIAL_LOG_FULL
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:99
  - isVerboseLogging()
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:100
  - isFullLogging()
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\mission_M5_refactored.ino:1716
  - boot log prints SERIAL_LOG_LEVEL

### 2.6 Free Roam 4-spot logic and dev-tag mapping
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:429
  - REQUIRED_LOCATIONS = 4
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\FreeRoamMission.cpp:84
  - buildMissionSelection()
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\FreeRoamMission.cpp:156
  - findDevSpotForTag()
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\FreeRoamMission.cpp:240
  - getCurrentDifficulty() (1-4 => Easy/Medium/Hard/Extreme)

Guru Home tag assignments (slot 1)
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:422
  - 04 F2 D2 7D CC 2A 81
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:423
  - 04 74 D3 7F CC 2A 81
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:424
  - 04 8C F2 7F CC 2A 81
- i:\Scenario Writing\MW5\Secondary\mission_M5_refactored\Config.h:425
  - 86 10 EA AD

## 3) API Changes for game_id Support

## 3.1 Problem before patch
The API accepted payloads at /iot/secondaryMissionComplete but resolved game context from widget.game only.
Any incoming game_id from the widget payload was ignored.

## 3.2 Route changes in iotRoutes.js
Updated to use optional game_id override with fallback to widget.game.

- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1075
  - requestedGameId read from req.body.game_id
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1087
  - effectiveGameId = requestedGameId || String(widget.game || '')
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1103
  - completeSecondaryMission called with gameId: effectiveGameId

- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1139
  - same for /secondaryMissionCompleteMulti
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1151
  - effectiveGameId computed
- i:\Scenario Writing\Creator Links\api-iot-main\routes\iotRoutes.js:1167
  - service called with gameId: effectiveGameId

Before route behavior:

    {
      "source_of_game": "widget.game",
      "request.game_id": "ignored"
    }

After route behavior:

    {
      "source_of_game": "request.game_id when present",
      "fallback": "widget.game when request.game_id missing"
    }

## 3.3 Route changes in secondaryMissionRoutes.js
Kept parity for /secondary paths:

- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:42
  - requestedGameId read for /complete
- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:84
  - effectiveGameId computed
- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:87
  - gameId passed as effectiveGameId

- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:110
  - requestedGameId read for /validBadge
- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:141
  - effectiveGameId computed
- i:\Scenario Writing\Creator Links\api-iot-main\routes\secondaryMissionRoutes.js:144
  - gameId passed as effectiveGameId

## 4) Validation Rules Still Enforced (Important)
Even with game_id override, service validation still requires badge/faction membership in target game:

- i:\Scenario Writing\Creator Links\api-iot-main\core\secondaryMissionRewardService.js:598
  - factions resolved for target gameId
- i:\Scenario Writing\Creator Links\api-iot-main\core\secondaryMissionRewardService.js:600
  - primary badge must belong to one of those factions
- i:\Scenario Writing\Creator Links\api-iot-main\core\secondaryMissionRewardService.js:884
  - explicit mismatch error if faction game != effective game

Implication:
- Same physical badge UUID can be reused, but backend must contain a valid badge record linked to a faction in the new game.

## 5) Test Payloads

### 5.1 Completion test payload (recommended)

    {
      "mac_address": "34:B7:DA:56:0E:D8",
      "serial_number": "ggdevtester1",
      "game_id": "69ae08e1e0e9cdc11c2548af",
      "difficulty": "Easy",
      "uuid": "8F3ABB1F",
      "uuids": []
    }

### 5.2 Legacy compatibility payload (still supported)

    {
      "mac_address": "34:B7:DA:56:0E:D8",
      "serial_number": "ggdevtester1",
      "difficulty": "Easy",
      "uuid": "8F3ABB1F",
      "uuids": []
    }

## 6) Expected Outcomes
- With game_id provided: mission resolution and reward logic use that game context.
- Without game_id: behavior remains legacy (uses widget.game).
- If badge is not present in target game faction scope: request fails with mismatch/not found validation messages.
