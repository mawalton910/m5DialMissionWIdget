# PR: Secondary Mission Game Context + Free Roam Flow Updates

## Summary
This PR updates both firmware and API behavior to support game migration without changing physical badges, while simplifying mission flow and improving diagnostics.

## Problem
- Widget completion requests were being processed against legacy game context in some cases.
- Payload previously did not include explicit game targeting.
- API secondary mission routes resolved game context from widget mapping only (`widget.game`).
- Existing flow still depended on mission card scanning.

## What Changed

### Firmware (mission_M5_refactored)
1. Mission timeout changed to 15 minutes.
2. Completion payload now includes `game_id`.
3. Free Roam now auto-starts after player confirmation (mission card step removed from normal flow).
4. Added GTA-style `MISSION STARTING` splash.
5. Added touch support for Yes/No on Confirm Players screen.
6. Logging/diagnostics improvements kept enabled (`SERIAL_LOG_FULL`) and DHCP mode used for hotspot compatibility.

### API (api-iot-main)
1. Secondary mission endpoints now accept optional `game_id` from request body.
2. Effective game resolution now uses:
   - `request.game_id` if provided
   - fallback to `widget.game` if not provided
3. Applied to both route families:
   - `/iot/secondaryMissionComplete`
   - `/iot/secondaryMissionCompleteMulti`
   - `/secondary/complete`
   - `/secondary/validBadge`
4. Backward compatibility preserved for existing clients that do not send `game_id`.

## Key Files

### Firmware
- `Config.h`
  - `MISSION_TIMEOUT_MIN = 15`
  - `SERIAL_LOG_LEVEL = SERIAL_LOG_FULL`
  - `REQUIRED_LOCATIONS = 4`
- `secrets.h`
  - `DEVICE_GAME_ID = "69ae08e1e0e9cdc11c2548af"`
  - `WIFI_STATIC_IP_ENABLED = 0`
- `mission_M5_refactored.ino`
  - completion payload now includes `game_id`
  - new flow entrypoint `startFreeRoamFlow(bool showSplash)`
  - `drawMissionStartingScreen()`
  - touch handler for confirm players
- `FreeRoamMission.cpp`
  - 4-spot selection logic and saved-state format handling
  - dev tag spot mapping

### API
- `routes/iotRoutes.js`
  - updated `/secondaryMissionComplete`
  - updated `/secondaryMissionCompleteMulti`
- `routes/secondaryMissionRoutes.js`
  - updated `/complete`
  - updated `/validBadge`

## Request/Response Contract

### Previous payload (firmware)
```json
{
  "mac_address": "34:B7:DA:56:0E:D8",
  "serial_number": "ggdevtester1",
  "difficulty": "Easy",
  "uuid": "8F3ABB1F",
  "uuids": []
}
```

### New payload (firmware)
```json
{
  "mac_address": "34:B7:DA:56:0E:D8",
  "serial_number": "ggdevtester1",
  "game_id": "69ae08e1e0e9cdc11c2548af",
  "difficulty": "Easy",
  "uuid": "8F3ABB1F",
  "uuids": []
}
```

### Effective game resolution (API)
```json
{
  "effective_game": "request.game_id if present, else widget.game"
}
```

## Compatibility / Risk
- Backward compatible for old clients (no `game_id`).
- Low risk for unrelated routes.
- Existing service-level validation remains strict:
  - badge must exist in faction scope for effective game
  - mismatch still returns validation error

## Test Plan
1. Firmware compile passes for `esp32:esp32:crabik_slot_esp32_s3`.
2. POST completion with `game_id` set to new game:
   - expect rewards/completion to resolve under new game context.
3. POST completion without `game_id`:
   - expect legacy behavior (uses `widget.game`).
4. Negative test:
   - send `game_id` where badge has no valid faction mapping in that game
   - expect mismatch/validation failure.
5. Device flow test:
   - badge scan -> confirm players -> auto-start Free Roam -> complete mission -> server 200.

## Rollback
- API rollback: revert route-level `requestedGameId/effectiveGameId` logic in:
  - `routes/iotRoutes.js`
  - `routes/secondaryMissionRoutes.js`
- Firmware rollback: remove `game_id` from completion payload in `mission_M5_refactored.ino` and redeploy previous binary.

## Notes for Ops
- Deploy API changes before field-testing firmware that sends `game_id`.
- Ensure target game contains valid faction + badge linkage for reused physical badge UUIDs.
