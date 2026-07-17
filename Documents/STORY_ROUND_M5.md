# Shared Story Round Assignment

The normal `MODE_MISSION_WIDGET` now uses a one-time NPC assignment. All eight
dials can run the same firmware. Each dial stores the scanned token that Creator
links to an NPC through **Story Mission Token / Card**.

## Device flow

1. On first boot, the dial displays **ASSIGN DIAL / SCAN NPC TAG**.
2. Scan the Loot/Item UUID linked to the intended NPC in Creator.
3. The normalized token is stored in device preferences and survives reboots
   and normal mission resets.
4. Scan the participating player badges.
5. Confirm the players on the dial.
6. The dial automatically posts the stored token to
   `POST /mission-computer/story-round` using its existing IoT MAC/serial
   identity.
7. The server resolves the token to the NPC and returns the active round's
   already-selected story chain and four ordered POIs.
8. The firmware maps those POI names to its local RFID location table and
   starts the existing `FreeRoamMission`.
9. Completion continues through the existing completion endpoint and reward
   path. No reward behavior is changed by this mode.

Use **NPC TAG / RESET** in the admin menu to clear the assignment and bind the
dial to another NPC. A full factory reset also clears it. The linked Loot/Item
does not require a special keyword when its UUID is explicitly assigned to an
NPC's `mission_card_uuid`.

## Manifest response

The route returns:

- `device`: IoT, widget, and game IDs
- `source`: scanned mission UUID, starter loot ID, NPC ID/name, and story keyword
- `round`: active round ID, name/type, start/end times, and keywords
- `mission`: selected generated mission ID/name/description, node count, and
  generation metadata
- `pois` and `locations`: the same four ordered POIs with IDs, names,
  latitude/longitude, keywords, and UUIDs
- `reward_structure`: player/faction inventory references and step reward
  summaries; `rewards_preserved` is true and `random_loot` is currently null
- `websocket`: explicitly reports that the manifest is fetched over HTTP and
  should be requested again after reconnect or a round change

The route returns `409 STORY_MISSION_NOT_READY` until the NPC flow has generated
and selected the shared mission for the active round. It returns
`409 STORY_MISSION_POI_COUNT_MISMATCH` when the selected chain does not contain
exactly four usable POIs. This prevents the dial from silently running a
different mission than the NPC.

## Important mapping requirement

The dial's local `LOCATION_NAMES[]` must match the server POI names after
punctuation/case normalization. If a generated POI is not in that table, the
dial displays `POI NOT ON DIAL` rather than starting with the wrong RFID map.
