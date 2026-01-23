# MeshBerry MeshCore Integration - Debug Log

## Project Goal
Replace Meshtastic's radio/mesh routing stack with MeshCore on LilyGo T-Deck while keeping the Meshtastic UI.

## Current Status (Jan 23, 2026)
- **Screen**: WORKING (SPI deadlock fixed)
- **Device**: Still rebooting in a loop
- **Latest fixes applied**: Need testing

---

## Issues Found and Fixed

### ISSUE 1: SPI DEADLOCK (FIXED - Screen now works)

**Problem**: MeshCore task handler wrapped its loop in `spiLock->lock()/unlock()`, but radio operations inside also acquire spiLock through `LockingArduinoHal::spiBeginTransaction()`. Since `concurrency::Lock` uses a non-recursive binary semaphore, the same task trying to lock twice causes deadlock.

**File**: `src/mesh/MeshCoreAdapter.cpp`

**Deadlock Chain**:
1. MeshCore task: `spiLock->lock()` ✅ (succeeds)
2. MeshCore task: `meshCoreAdapter->loop()`
3. → `Mesh::loop()` → `Dispatcher::loop()` → `checkRecv()` → `_radio->recvRaw()`
4. → RadioLib `startReceive()` → Module SPI operations
5. → `LockingArduinoHal::spiBeginTransaction()` → `spiLock->lock()` ❌ **DEADLOCK**

**Fix Applied**: Removed `spiLock->lock()/unlock()` from task handler. LockingArduinoHal already handles SPI mutex internally.

```cpp
// BEFORE (caused deadlock):
static void meshCoreTaskHandler(void* param) {
    esp_task_wdt_add(NULL);
    while (true) {
        esp_task_wdt_reset();
        spiLock->lock();        // ❌ DEADLOCK
        if (meshCoreAdapter) {
            meshCoreAdapter->loop();
        }
        spiLock->unlock();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// AFTER (fixed):
static void meshCoreTaskHandler(void* param) {
    esp_task_wdt_add(NULL);
    while (true) {
        esp_task_wdt_reset();
        meshCoreLoopCount++;
        if (meshCoreLoopCount % 200 == 0) {
            LOG_DEBUG("MeshCore: loop count %u", meshCoreLoopCount);
        }
        // NOTE: Do NOT acquire spiLock here!
        // LockingArduinoHal::spiBeginTransaction() already acquires spiLock
        if (meshCoreAdapter) {
            meshCoreAdapter->loop();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

---

### ISSUE 2: UNINITIALIZED VARIABLES IN DISPATCHER (FIXED)

**Problem**: `outbound_start` and `outbound_expiry` not initialized in Dispatcher constructor, causing garbage values that could trigger immediate timeout conditions.

**File**: `lib/MeshCore/src/Dispatcher.h` (lines 117, 134-145)

**Impact**: In `Dispatcher::loop()`:
- Line 63: `long t = _ms->getMillis() - outbound_start;` → garbage calculation
- Line 79: `millisHasNowPassed(outbound_expiry)` → may immediately trigger timeout

**Fix Applied**: Added initialization in constructor.

```cpp
// BEFORE:
Dispatcher(Radio& radio, MillisecondClock& ms, PacketManager& mgr)
  : _radio(&radio), _ms(&ms), _mgr(&mgr)
{
    outbound = NULL;
    total_air_time = rx_air_time = 0;
    next_tx_time = 0;
    cad_busy_start = 0;
    next_floor_calib_time = next_agc_reset_time = 0;
    // outbound_start = ???  NOT INITIALIZED!
    // outbound_expiry = ??? NOT INITIALIZED!
    _err_flags = 0;
    radio_nonrx_start = 0;
    prev_isrecv_mode = true;
}

// AFTER:
Dispatcher(Radio& radio, MillisecondClock& ms, PacketManager& mgr)
  : _radio(&radio), _ms(&ms), _mgr(&mgr)
{
    outbound = NULL;
    outbound_start = outbound_expiry = 0;  // MUST initialize to prevent garbage values
    total_air_time = rx_air_time = 0;
    next_tx_time = 0;
    cad_busy_start = 0;
    next_floor_calib_time = next_agc_reset_time = 0;
    _err_flags = 0;
    radio_nonrx_start = 0;
    prev_isrecv_mode = true;
}
```

---

### ISSUE 3: BUSY-LOOP IN recvRaw() (FIXED)

**Problem**: If `startReceive()` fails, state never changes to STATE_RX, causing continuous retries without delay - potential watchdog timeout.

**File**: `lib/MeshCore/src/helpers/radiolib/RadioLibWrappers.cpp` (lines 116-123)

**Fix Applied**: Added 100ms delay backoff when startReceive() fails.

```cpp
// BEFORE:
if (state != STATE_RX) {
    int err = _radio->startReceive();
    if (err == RADIOLIB_ERR_NONE) {
        state = STATE_RX;
    } else {
        MESH_DEBUG_PRINTLN("RadioLibWrapper: error: startReceive(%d)", err);
        // No delay - immediate retry = busy loop!
    }
}

// AFTER:
if (state != STATE_RX) {
    int err = _radio->startReceive();
    if (err == RADIOLIB_ERR_NONE) {
        state = STATE_RX;
    } else {
        MESH_DEBUG_PRINTLN("RadioLibWrapper: error: startReceive(%d)", err);
        // Backoff to prevent busy-loop when radio fails to enter RX mode
        delay(100);
    }
}
```

---

## Other Fixes Applied (router-> NULL guards)

All `router->` usages have been guarded with `#ifdef USE_MESHCORE` or `if (router)` checks:

| File | Line(s) | Guard Type |
|------|---------|------------|
| RoutingModule.cpp | 31-32 | `#ifndef USE_MESHCORE` |
| PhoneAPI.cpp | 230, 795, 801, 811, 820 | `if (router)` |
| TraceRouteModule.cpp | 591, 712 | `#ifdef USE_MESHCORE return` |
| SerialModule.cpp | 321 | `#ifdef USE_MESHCORE return` |
| SerialModule.h | 76 | `#ifdef USE_MESHCORE return nullptr` |
| MQTT.cpp | 91-95, 188-196, 224-231 | `#ifndef USE_MESHCORE` |
| MenuApplet.cpp | 848 | `#ifdef USE_MESHCORE return` |
| SimRadio.cpp | 226-228 | `if (router)` |
| MeshModule.cpp | 62 | `#ifdef USE_MESHCORE return nullptr` |
| SinglePortModule.h | 37 | `#ifdef USE_MESHCORE return nullptr` |
| ProtobufModule.h | 45-47 | `#ifdef USE_MESHCORE return nullptr` |
| DeviceTelemetry.cpp | 140-141 | `if (router)` |
| UdpMulticastHandler.h | 64 | `router &&` check |
| RadioInterface.cpp | 705 | `if (router)` |

---

## Other Modifications

### Stack Size Increase
**File**: `src/mesh/MeshCoreAdapter.cpp`
- Changed MeshCore task stack from 4096 to 8192 bytes to prevent stack overflow

### Watchdog Integration
**File**: `src/mesh/MeshCoreAdapter.cpp`
- Added `esp_task_wdt_add(NULL)` and `esp_task_wdt_reset()` to MeshCore task

### Debug Logging
**File**: `src/mesh/MeshCoreAdapter.cpp`
- Added counter that logs "MeshCore: loop count X" every ~10 seconds (200 iterations)

---

## Files Modified (Summary)

| File | Changes |
|------|---------|
| `src/mesh/MeshCoreAdapter.cpp` | Removed spiLock deadlock, added watchdog, debug logging, increased stack |
| `lib/MeshCore/src/Dispatcher.h` | Initialize outbound_start and outbound_expiry to 0 |
| `lib/MeshCore/src/helpers/radiolib/RadioLibWrappers.cpp` | Added delay(100) backoff on startReceive error |
| `src/mesh/ProtobufModule.h` | Added USE_MESHCORE guard returning nullptr |
| Various files | Added router-> null guards (see table above) |

---

## Build Commands

```bash
cd meshtastic_base/firmware

# Clean build
pio run -e t-deck-tft -t clean

# Build only
pio run -e t-deck-tft

# Build and flash
pio run -e t-deck-tft -t upload

# Monitor serial
pio device monitor -b 115200
```

---

## Verification Steps

1. Flash firmware
2. Monitor serial output for 2+ minutes
3. **Expected behavior**:
   - Screen displays Meshtastic UI (confirms SPI working)
   - "MeshCore: loop count X" messages every ~10 seconds
   - No reboot loops
   - Device runs stably

4. **Look for in serial output**:
   - `MeshCore: Initializing...`
   - `MeshCore: Task started on Core 1 (priority 0)`
   - `MeshCore: loop count 200` (after ~10 seconds)
   - `MeshCore: loop count 400` (after ~20 seconds)
   - etc.

---

## If Still Rebooting - Next Steps to Investigate

1. **Check for other uninitialized variables** in MeshCore library
2. **Look at Dispatcher::loop()** (`lib/MeshCore/src/Dispatcher.cpp`) for potential issues
3. **Check if radio initialization actually succeeds** - look for error codes
4. **Memory issues** - heap corruption, stack overflow elsewhere
5. **Add more debug logging** in MeshCore to pinpoint where crash occurs

### Potential Areas to Investigate

- `lib/MeshCore/src/Dispatcher.cpp` - main loop logic
- `lib/MeshCore/src/Mesh.cpp` - mesh protocol handling
- Radio initialization in `initMeshCore()` - check return codes
- Packet allocation/deallocation in `StaticPoolPacketManager`

---

## Architecture Notes

### Task Structure
- **MeshCore task**: Runs on Core 1, priority 0, 8192 byte stack
- **TFT/Display task**: Runs on Core 0, priority 1
- Both share SPI bus via `spiLock` mutex (handled by LockingArduinoHal)

### SPI Locking
- `concurrency::Lock` uses binary semaphore (NOT recursive)
- `LockingArduinoHal` wraps each SPI transaction with lock/unlock
- Do NOT add additional locking around code that uses LockingArduinoHal

### MeshCore Integration Points
- `MeshCoreAdapter` extends `mesh::Mesh`
- `MeshCoreSX1262Wrapper` wraps RadioLib's SX1262
- Uses Meshtastic's `LockingArduinoHal` for SPI mutex protection
- `USE_MESHCORE` preprocessor flag gates code paths

---

## Key Files Reference

```
meshtastic_base/firmware/
├── src/mesh/
│   ├── MeshCoreAdapter.cpp      # Main integration adapter
│   └── MeshCoreAdapter.h
├── lib/MeshCore/src/
│   ├── Dispatcher.h             # Packet scheduling (MODIFIED)
│   ├── Dispatcher.cpp
│   ├── Mesh.h
│   ├── Mesh.cpp
│   └── helpers/radiolib/
│       └── RadioLibWrappers.cpp # Radio abstraction (MODIFIED)
└── src/concurrency/
    ├── Lock.h                   # Binary semaphore wrapper
    └── Lock.cpp
```
