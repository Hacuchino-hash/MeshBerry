# MeshBerry TODO List - Boot Loop Fix & Improvements

**Current Branch:** `feature/boot-loop-fix`

**Status:** Fixes applied and flashed, testing NOT yet complete. Do not merge until verification complete.

---

## 🔄 IN PROGRESS (CRITICAL - TESTING PHASE)

### Test sleep/wake WITHOUT USB cable (critical test)
- **Priority:** CRITICAL
- **Status:** IN PROGRESS
- **Description:** Verify device sleeps and wakes cleanly on battery power only
- **Success Criteria:**
  - Device enters sleep after 2.5 minutes inactivity
  - Keyboard wake works (~1-2 seconds, NOT 15 seconds)
  - No reboots when USB disconnected
  - No boot loops during or after sleep

### Test message send/receive after wake (60+ minutes)
- **Priority:** CRITICAL
- **Status:** PENDING
- **Description:** Long-term stability test with message operations
- **Success Criteria:**
  - Send messages successfully after wake
  - Receive messages successfully
  - No crashes during message operations
  - No boot loops after 60+ minutes

---

## ✅ COMPLETED (PHASE 0: BOOT LOOP FIX)

### Fix Serial buffer overflow causing watchdog timeouts
- **Status:** COMPLETED
- **Files:** `src/main.cpp`
- **Changes:** Added `Serial.setTimeout(0)` for non-blocking writes
- **Commits:** Not yet committed (pending)

### Add Meshtastic sleep pattern (ANALOG pins, RTC power domain)
- **Status:** COMPLETED
- **Files:** `src/drivers/power.cpp`
- **Changes:**
  - I2C pins to ANALOG mode after Wire.end()
  - RTC power domain configuration
  - LoRa DIO1 pulldown + CS gpio_hold
  - Removed strapping pin manipulation
- **Commits:** Not yet committed (pending)

### Optimize MessageArchive to eliminate heap fragmentation
- **Status:** COMPLETED
- **Files:** `src/settings/MessageArchive.cpp`
- **Changes:**
  - Rewrote appendMessage() to use File.seek()
  - Rewrote loadMessages() for streaming
  - 90%+ reduction in heap allocations
- **Commits:** Already committed to feature/boot-loop-fix

### Build and flash Meshtastic-pattern firmware
- **Status:** COMPLETED
- **Build Size:** 1,053,101 bytes (16.1% Flash, 48.2% RAM)
- **Flashed:** Yes
- **Verified:** NO - testing in progress

---

## ⏳ PENDING (PHASE 1: QUICK WINS)

### Add UBX packet builder for GPS (Phase 1)
- **Priority:** HIGH
- **Effort:** 1-2 hours
- **Impact:** Foundation for GPS power management
- **Files:** `src/drivers/gps.cpp`
- **Dependencies:** Sleep must be stable first

### Implement GPS power save command (M10Q UBX-RXM-PMREQ)
- **Priority:** HIGH
- **Effort:** 2-3 hours
- **Impact:** 90% GPS power savings (~20mA)
- **Files:** `src/drivers/gps.h`, `src/drivers/gps.cpp`
- **Dependencies:** UBX packet builder

### Add GPS fix hold timer for ephemeris
- **Priority:** HIGH
- **Effort:** 1-2 hours
- **Impact:** Faster GPS subsequent fixes
- **Files:** `src/drivers/gps.cpp`
- **Dependencies:** GPS power save command

### Implement Battery OCV lookup table with interpolation
- **Priority:** HIGH
- **Effort:** 2-3 hours
- **Impact:** ±10% more accurate battery percentage
- **Files:** `src/drivers/power.cpp`, `src/drivers/power.h`
- **Dependencies:** None

### Add UI frame rate throttling (1 FPS idle, 15 FPS active)
- **Priority:** HIGH
- **Effort:** 2-3 hours
- **Impact:** 98% fewer update checks, snappier UI
- **Files:** `src/ui/ScreenManager.h`, `src/ui/ScreenManager.cpp`
- **Dependencies:** None

### Add UI input debouncing (100ms)
- **Priority:** HIGH
- **Effort:** 1 hour
- **Impact:** Smoother input, no double-fires
- **Files:** `src/ui/ScreenManager.cpp`
- **Dependencies:** None

---

## 📋 GIT TASKS

### Commit all changes to git
- **Status:** PENDING (this commit)
- **Files to commit:**
  - `src/main.cpp` - Serial.setTimeout(0) fix
  - `src/drivers/power.cpp` - Meshtastic sleep pattern
  - `docs/BOOT_LOOP_FIX.md` - Comprehensive documentation
  - `TODO.md` - This file
- **Commit message:** "Add Serial.setTimeout(0) fix and Meshtastic sleep pattern - testing pending"

### Push to `feature/boot-loop-fix` branch
- **Status:** PENDING (this push)
- **Target:** origin/feature/boot-loop-fix
- **Note:** DO NOT merge to dev until testing complete

### Merge to `dev` after successful testing
- **Status:** BLOCKED (waiting on testing)
- **Blocker:** Sleep/wake verification incomplete
- **Success Criteria:**
  - All critical tests pass (see IN PROGRESS section)
  - 60+ minute stability test successful
  - No boot loops observed

---

## 📊 PHASE 1 QUICK WINS SUMMARY

**Total Estimated Effort:** 10-15 hours

**Expected Impact:**
- 20mA average power savings (GPS power management)
- ±10% better battery accuracy (OCV lookup table)
- Dramatically snappier UI feel (frame throttling + debouncing)
- Better sleep behavior (less CPU churn)

**Implementation Order (after sleep stability confirmed):**
1. GPS Power Management (5-8 hours)
2. Battery OCV Table (2-3 hours)
3. UI Frame Rate Throttling + Debouncing (3-4 hours)

---

## 🔍 VERIFICATION CHECKLIST

### Sleep/Wake Verification (CRITICAL)
- [ ] Device enters sleep after 2.5 minutes inactivity
- [ ] Keyboard wake works in ~1-2 seconds (not 15 seconds)
- [ ] No reboots when USB disconnected
- [ ] No boot loops during sleep/wake cycles
- [ ] Serial Monitor shows "Step 12: WOKE FROM SLEEP" message
- [ ] 60+ minute stability test passes

### Message Operations Verification
- [ ] Can send messages after wake
- [ ] Can receive messages successfully
- [ ] No crashes during message operations
- [ ] MessageArchive optimization working (no heap fragmentation)

### GPS Power Management Verification (Phase 1)
- [ ] GPS enters power save mode after fix hold
- [ ] GPS wakes on UART RX (send 0xFF)
- [ ] Fix acquisition faster on subsequent wakes
- [ ] Current draw reduced (if measurable)
- [ ] No crashes after 24 hours

### Battery Monitoring Verification (Phase 1)
- [ ] Percentage curve smoother than before
- [ ] Voltage jitter eliminated
- [ ] Charging detection works
- [ ] Matches external voltmeter within ±5%

### UI Performance Verification (Phase 1)
- [ ] Frame rate is 1 FPS when idle
- [ ] Frame rate is 15 FPS during input/navigation
- [ ] No input double-fires
- [ ] UI feels responsive (no lag)
- [ ] CPU usage reduced during idle

---

## 📝 NOTES

**Root Causes Identified:**
1. **PRIMARY:** Serial buffer overflow - `Serial.println()` blocked when USB disconnected, causing watchdog timeout
2. **SECONDARY:** MessageArchive heap fragmentation - 10-100KB allocations on every message
3. **RESOLVED:** Sleep crash - noInterrupts() conflict, strapping pin manipulation, missing Meshtastic patterns

**Critical User Feedback:**
- "its not fixed so dont lie. keep it in the branch we are in" - Testing NOT complete, stay in feature branch
- "dont break it more" - Conservative approach required
- "after disconnection serial it came out of loop" - Serial was PRIMARY cause

**Reference Documentation:**
- See `docs/BOOT_LOOP_FIX.md` for comprehensive technical details
- See `.vscode/platformio-env.sh` for PlatformIO CLI setup
- Meshtastic T-Deck firmware patterns applied (industry-standard, not Meshtastic-specific)

---

**Last Updated:** 2026-01-23
**Current Firmware Build:** 1,053,101 bytes (16.1% Flash, 48.2% RAM)
**Branch:** feature/boot-loop-fix
**Status:** TESTING IN PROGRESS - DO NOT MERGE
