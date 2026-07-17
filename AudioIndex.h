#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  AudioIndex.h — Master audio reference for mission_M5_refactored
//
//  This is a READ-ONLY reference file. It does not define any sounds.
//  All actual definitions live in Sounds.h (buzzer) and the external
//  AudioPlayer SD card. Edit those sources to change sounds.
//
//  Two audio systems are in use:
//    1. Internal Buzzer  — M5Dial.Speaker.tone()  via playSound() in Sounds.h
//    2. External AudioPlayer — M5Stack U197 Unit on Port B (UART, N9301 chip)
//       Files on microSD are played by index: audioPlayer.playAudioByIndex(n)
// ═══════════════════════════════════════════════════════════════════════════

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  INTERNAL BUZZER — Sounds.h                                            │
// │  Played via: playSound(sequence, length, volume, "name")               │
// │  All are blocking tone sequences using M5Dial.Speaker.tone()           │
// ├──────────────────────┬──────────────────────────────────────────────────┤
// │  Constant            │  Trigger / Action                               │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_BADGE_SCAN      │  Player badge scanned (rising 3-note blip)      │
// │                      │  → playSuccessTone() wrapper                    │
// │                      │  → also fires on green/success displayMessage() │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_MISSION_START   │  Heist mission card scanned (C4→C6 sweep)       │
// │                      │  → processCardScan() WAIT_FOR_MISSION_CARD      │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_LOCATION_VISIT  │  POI location tag scanned (triple E6 chirp)     │
// │                      │  → playAcceptTone() wrapper                     │
// │                      │  → also fires on blue/info displayMessage()     │
// │                      │  → also fires on state changes when LOUD_MODE   │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_MISSION_COMPLETE│  Mission rewards sent successfully              │
// │                      │  → sendCombinedCompletionRequest() on success   │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_TIMEOUT         │  Mission timer expired (sad descent F4→G3)      │
// │                      │  → update() timer expiry handler                │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_ERROR           │  Invalid tag / bad scan (3x 200Hz buzz)         │
// │                      │  → playErrorTone() wrapper                      │
// │                      │  → also fires on red/error displayMessage()     │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_MISSION_SCRUBBED│  GM scrubs an active mission (descending drop)  │
// │                      │  → processCardScan() scrub tag handler          │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_BADGE_COOLDOWN  │  Blocked/scrubbed badge scanned (wobble sweep)  │
// │                      │  → processCardScan() cooldown check             │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_BADGE_REMOVED   │  Player badge de-registered (descending drop)   │
// │                      │  → playRemoveTone() wrapper                     │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_RESET           │  Device reset (C5 → E4)                         │
// │                      │  → reset() / fullReset()                        │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_SAFE_CONFIRM    │  Safe Cracker — dial landed on exact number     │
// │                      │  → playSafeConfirmTone() inline helper          │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_SAFE_PROX       │  Safe Cracker — proximity feedback beep         │
// │                      │  → playSafeProxTone(freq) with runtime freq     │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_SENDING_DOTS    │  HTTP send animation (3 ascending dot blips)    │
// │                      │  → playSendingDot(i) for i=0,1,2               │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_SERVER_HOLD     │  Primary POST sending tone (C5→C6 arpeggio)    │
// │                      │  → startSendHoldTone()                          │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_RESOURCE_HOLD   │  Resource bonus POST tone (same, quieter)       │
// │                      │  → startResourceHoldTone()                      │
// ├──────────────────────┼──────────────────────────────────────────────────┤
// │  SND_SERVER_RETRY    │  Failed HTTP retry buzz (single 200Hz pulse)    │
// │                      │  → between retry attempts in mission sends      │
// └──────────────────────┴──────────────────────────────────────────────────┘

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  EXTERNAL AUDIOPLAYER — M5Stack U197 (microSD)                          │
// │  Played via: audioPlayer.playAudioByIndex(n)                           │
// │  Files must be named 001.mp3, 002.mp3, etc. on the microSD card       │
// │  Play modes: SINGLE_STOP (one-shot) or SINGLE_LOOP (looping)          │
// ├──────┬─────────────┬──────────────────────────────────────────────────┤
// │  Idx │  SD File     │  Trigger / Action                               │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  001 │  boot        │  Boot splash screen (mission system)            │
// │      │              │  → displaySplashScreen()                        │
// │      │              │  Mode: SINGLE_STOP                              │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  002 │  badge_scan  │  Player badge scanned (rising 3-note blip)      │
// │      │              │  → playSuccessTone() / EXT_BADGE_SCAN           │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 800→1200→1600 Hz              │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  003 │  mission_start│ Heist card scanned (C4→C6 power sweep)        │
// │      │              │  → EXT_MISSION_START                            │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: C4→G4→C5→G5→C6               │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  004 │  location    │  POI location tag scanned (triple E6 chirp)     │
// │      │              │  → playAcceptTone() / EXT_LOCATION_VISIT        │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 3x 1319 Hz (E6)              │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  005 │  complete    │  Mission rewards sent successfully              │
// │      │              │  → EXT_MISSION_COMPLETE                         │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: D5-C5-Bb4 x3 + Ab4           │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  006 │  timeout     │  Mission timer expired (sad descent)            │
// │      │              │  → EXT_TIMEOUT                                  │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: F4→C#4→G3                     │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  007 │  error       │  Invalid tag / bad scan (3x buzz)               │
// │      │              │  → playErrorTone() / EXT_ERROR                  │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 3x 200 Hz                     │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  008 │  scrubbed    │  GM scrubs active mission (descending drop)     │
// │      │              │  → EXT_MISSION_SCRUBBED                         │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 600→400→200 Hz                │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  009 │  cooldown    │  Blocked/scrubbed badge (wobble sweep)          │
// │      │              │  → EXT_BADGE_COOLDOWN                           │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 400→1000→400 Hz sweep         │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  010 │  removed     │  Player badge de-registered (descending drop)   │
// │      │              │  → playRemoveTone() / EXT_BADGE_REMOVED         │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 1600→1200→800 Hz              │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  011 │  reset       │  Device reset (C5 → E4)                         │
// │      │              │  → EXT_RESET                                    │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 523→330 Hz                    │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// │  012 │  retry       │  Failed HTTP retry buzz (low pulse)             │
// │      │              │  → EXT_SERVER_RETRY                             │
// │      │              │  Mode: SINGLE_STOP                              │
// │      │              │  Buzzer fallback: 200 Hz 200ms                  │
// ├──────┼─────────────┼──────────────────────────────────────────────────┤
// └──────┴─────────────┴──────────────────────────────────────────────────┘

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  NOTES                                                                 │
// ├─────────────────────────────────────────────────────────────────────────┤
// │  • SD card indices 001 = boot splash, 002–012 = mission sounds.      │
// │                                                                        │
// │  • Mission sounds (002–012) are activated by passing EXT_* constants   │
// │    to playSound(). If AudioPlayer is connected, the .mp3 plays         │
// │    (non-blocking). Otherwise the buzzer sequence plays (blocking).     │
// │                                                                        │
// │  • Buzzer sounds (Sounds.h) and the AudioPlayer use different         │
// │    hardware — they can play simultaneously but generally shouldn't.    │
// │                                                                        │
// │  • AudioPlayer volume is set once at boot: audioPlayer.setVolume(20)   │
// │    Buzzer volume is per-call via playSound() volume parameter.         │
// │                                                                        │
// │  • Buzzer sounds are BLOCKING (delay-based). AudioPlayer sounds are    │
// │    non-blocking (fire-and-forget via UART command).                    │
// └─────────────────────────────────────────────────────────────────────────┘
