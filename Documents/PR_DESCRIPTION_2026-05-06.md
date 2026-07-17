# PR: Secondary Mission API Game Context Override

## Summary
This PR updates secondary mission API routes to support an optional game_id override in request payloads. If provided, game_id is used as the effective game context. If omitted, behavior remains unchanged and falls back to widget.game.

## Problem
- Secondary mission requests were always resolved against widget.game.
- Incoming game_id from client payloads was ignored.
- This prevented controlled migration/testing against a different game context without reassigning widget mappings first.

## What Changed

### API route behavior
1. Added optional request parsing for game_id.
2. Added effective game resolution logic:
   - Use request.game_id when present and non-empty.
   - Otherwise use widget.game.
3. Passed effectiveGameId into secondary mission service calls.

### Endpoints updated
1. /iot/secondaryMissionComplete
2. /iot/secondaryMissionCompleteMulti
3. /secondary/complete
4. /secondary/validBadge

## Key Files

### routes/iotRoutes.js
- Updated /secondaryMissionComplete route to resolve effectiveGameId.
- Updated /secondaryMissionCompleteMulti route to resolve effectiveGameId.

### routes/secondaryMissionRoutes.js
- Updated /complete route to resolve effectiveGameId.
- Updated /validBadge route to resolve effectiveGameId.

## Contract Examples

### Legacy request (still supported)
```json
{
  "mac_address": "34:B7:DA:56:0E:D8",
  "serial_number": "ggdevtester1",
  "difficulty": "Easy",
  "uuid": "8F3ABB1F",
  "uuids": []
}
```

### Request with explicit game override
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

### Effective game resolution
```json
{
  "effective_game": "request.game_id if present, else widget.game"
}
```

## Compatibility / Risk
- Backward compatible for clients that do not send game_id.
- Low blast radius: changes are scoped to secondary mission route handlers.
- Existing service validation remains unchanged:
  - badge must exist in faction scope for the effective game
  - mismatch conditions still return validation errors

## Test Plan
1. POST /iot/secondaryMissionComplete without game_id.
   - Expect legacy behavior (game resolves from widget.game).
2. POST /iot/secondaryMissionComplete with game_id.
   - Expect service logic and reward resolution under game_id context.
3. POST /iot/secondaryMissionCompleteMulti with and without game_id.
   - Expect same fallback/override behavior.
4. POST /secondary/complete and /secondary/validBadge with and without game_id.
   - Expect same fallback/override behavior.
5. Negative validation test.
   - Send game_id where badge/faction linkage is missing for that game.
   - Expect validation failure (mismatch/not found).

## Rollback
- Revert route-level requestedGameId/effectiveGameId logic in:
  - routes/iotRoutes.js
  - routes/secondaryMissionRoutes.js

## Notes for Ops
- Safe to deploy independently of firmware changes due to fallback behavior.
- For explicit game override flows, ensure badge and faction records exist in target game context.
