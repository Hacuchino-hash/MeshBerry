# Direct Message (DM) Routing Analysis
## MeshBerry Feature Branch: feature/lora-routing

**Date**: January 21, 2026
**Goal**: Fix DM routing to work reliably through multiple hops
**Status**: Investigation complete, issues identified

---

## ✅ Current Implementation Status

### What's Already Working

Your DM implementation **already has the correct structure** from MeshCore:

```cpp
// In sendDirectMessage() - line 1314-1324
bool useDirect = isPathValid(peer.outPathLen, peer.pathLearnedAt);
if (useDirect) {
    sendDirect(pkt, peer.outPath, peer.outPathLen);  // ✅ Uses MeshCore's sendDirect!
    Serial.printf("[DM] Message sent via DIRECT route (%d hops)\n", peer.outPathLen);
} else {
    sendFlood(pkt);  // Fallback if no path
    Serial.println("[DM] Message sent via FLOOD (no path known)");
}
```

This matches MeshCore's `BaseChatMesh::sendMessage()` pattern exactly!

### Path Learning Mechanisms (Already Implemented)

1. **From ACK packets** (line 521-524):
   ```cpp
   if (packet->isRouteFlood() && packet->path_len > 0) {
       learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
   }
   ```

2. **From PATH_RETURN packets** (line 1021):
   ```cpp
   learnPath(contactId, path, path_len);
   ```

---

## 🐛 The Problem: Why DMs Fail Through Hops

Based on the code analysis, here are the **likely issues**:

### Issue #1: Packet Forwarding May Not Be Enabled

**Location**: `allowPacketForward()` - line 718
```cpp
bool MeshBerryMesh::allowPacketForward(const mesh::Packet* packet) {
    return _forwardingEnabled;  // Simple boolean check
}
```

**Problem**: If `_forwardingEnabled` is false (default?), intermediate nodes won't relay DIRECT-routed packets!

**MeshCore Behavior**: Intermediate nodes check `allowPacketForward()` before relaying. If it returns false, packets stop.

**Fix**: Ensure forwarding is enabled, OR make it smarter (always forward DIRECT packets even if flood forwarding is disabled).

### Issue #2: Path Learning Only From Flood ACKs

**Location**: `onAckRecv()` - line 522
```cpp
if (packet->isRouteFlood() && packet->path_len > 0) {  // ⚠️ Only flood!
    learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
}
```

**Problem**:
- Paths are only learned from FLOOD-routed ACKs
- Direct-routed ACKs (`packet->isRouteDirect()`) are ignored!
- This creates a catch-22: need path to send direct, but only learn path from flood

**MeshCore Behavior**: ACKs can come back via either route. Direct ACKs also contain path info.

**Fix**: Learn paths from **both** flood AND direct ACKs:
```cpp
if (packet->path_len > 0) {  // Accept any ACK with path info
    learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
}
```

### Issue #3: Initial Path Discovery

**Current Flow**:
1. Send DM #1 → uses flood (no path known yet)
2. Receive ACK with path → learn path
3. Send DM #2 → uses direct route ✅

**Problem**: If ACK gets lost in step 2, we never learn the path!

**MeshCore Solution**: Uses `PATH_RETURN` packets explicitly:
- After receiving a message, recipient can send back a PATH_RETURN
- This is separate from ACKs and specifically for path discovery
- More reliable than relying on ACKs alone

### Issue #4: Path Expiration Too Aggressive?

**Location**: `isPathValid()` - line 1379
```cpp
uint32_t age = millis() - learnedAt;
if (age > PATH_EXPIRY_MS) {  // What is PATH_EXPIRY_MS?
    return false;
}
```

**Question**: What's the expiry time? If too short, paths expire before second message.

### Issue #5: Intermediate Node Routing

**Critical MeshCore code**: `Mesh.cpp` line 80-96
```cpp
if (pkt->isRouteDirect() && pkt->path_len >= PATH_HASH_SIZE) {
    if (self_id.isHashMatch(pkt->path) && allowPacketForward(pkt)) {
        if (!_tables->hasSeen(pkt)) {
            removeSelfFromPath(pkt);  // Remove ourselves from path
            return ACTION_RETRANSMIT_DELAYED(0, d);  // Forward it!
        }
    }
    return ACTION_RELEASE;  // Not for us, discard
}
```

**How it works**:
1. Check if packet is DIRECT-routed
2. Check if **our hash** is at the front of the path
3. If yes, remove ourselves and forward
4. If no, discard (not our hop)

**Verification Needed**: Are your intermediate nodes doing this? Check `onRecvPacket()` logic.

---

## 🔍 Debugging Steps to Identify Root Cause

### Step 1: Enable Debug Logging

Add these Serial prints to track what's happening:

```cpp
// In sendDirectMessage()
Serial.printf("[DM-DEBUG] outPathLen=%d, pathValid=%s\n",
              peer.outPathLen, useDirect ? "YES" : "NO");
if (useDirect) {
    Serial.print("[DM-DEBUG] Path: ");
    for (int i = 0; i < peer.outPathLen; i++) {
        Serial.printf("%02X ", peer.outPath[i]);
    }
    Serial.println();
}

// In allowPacketForward()
Serial.printf("[FORWARD-DEBUG] Packet type=0x%02X, route=%s, forwarding=%s\n",
              packet->getPayloadType(),
              packet->isRouteDirect() ? "DIRECT" : "FLOOD",
              _forwardingEnabled ? "ENABLED" : "DISABLED");

// In onRecvPacket() for direct packets
if (packet->isRouteDirect()) {
    Serial.printf("[RECV-DEBUG] Direct packet, path_len=%d, first_hop=%02X, our_hash=%02X\n",
                  packet->path_len,
                  packet->path_len > 0 ? packet->path[0] : 0,
                  self_id.hash[0]);
}
```

### Step 2: Test Scenarios

**Scenario A: Direct Connection (No Hops)**
- Device A → Device B (within range)
- **Expected**: Should work even without forwarding
- **Test**: Send DM, check if received

**Scenario B: One Hop Through Repeater**
- Device A → Repeater → Device B
- **Expected**:
  1. First DM uses flood
  2. ACK comes back with path
  3. Second DM uses direct route
- **Test**:
  - Check repeater logs for forwarding
  - Verify path was learned from ACK
  - Verify second DM uses direct

**Scenario C: Two Hops**
- Device A → Repeater 1 → Repeater 2 → Device B
- **Expected**: Same as scenario B but longer path
- **Test**: Verify each hop forwards correctly

### Step 3: Check Configuration

```cpp
// In main.cpp or init code, verify:
mesh.setForwardingEnabled(true);  // ⚠️ Is this set?

// Check PATH_EXPIRY_MS value
Serial.printf("[CONFIG] PATH_EXPIRY_MS = %lu\n", PATH_EXPIRY_MS);

// Check if nodes are configured as repeaters
Serial.printf("[CONFIG] Node type = %d (2=REPEATER)\n", /* node type */);
```

---

## 🛠️ Proposed Fixes

### Fix #1: Always Allow Direct Packet Forwarding

**File**: `src/mesh/MeshBerryMesh.cpp`
**Function**: `allowPacketForward()`
**Line**: ~718

**Current**:
```cpp
bool MeshBerryMesh::allowPacketForward(const mesh::Packet* packet) {
    return _forwardingEnabled;
}
```

**Proposed**:
```cpp
bool MeshBerryMesh::allowPacketForward(const mesh::Packet* packet) {
    // Always forward DIRECT-routed packets (they have explicit paths)
    // This is safe because DIRECT packets only go to nodes in the path
    if (packet->isRouteDirect()) {
        return true;
    }

    // For FLOOD packets, respect the forwarding setting
    return _forwardingEnabled;
}
```

**Rationale**: Direct routing is explicit and controlled. No risk of forwarding loops.

### Fix #2: Learn Paths From All ACKs

**File**: `src/mesh/MeshBerryMesh.cpp`
**Function**: `onAckRecv()`
**Line**: ~522

**Current**:
```cpp
if (packet->isRouteFlood() && packet->path_len > 0) {
    learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
}
```

**Proposed**:
```cpp
// Learn path from any ACK that has path info
if (packet->path_len > 0) {
    learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
    Serial.printf("[ROUTE] Learned path from %s ACK: %d hops\n",
                  packet->isRouteFlood() ? "FLOOD" : "DIRECT",
                  packet->path_len);
}
```

**Rationale**: ACKs can return via either route. Both contain useful path info.

### Fix #3: Implement PATH_RETURN for Reliability (Optional But Recommended)

**Pattern from MeshCore**: When receiving a DM, explicitly send back a PATH_RETURN packet.

**In `onPeerDataRecv()` after receiving a DM**:
```cpp
// If we don't have a path to sender, or path is old, send PATH_RETURN
if (sender_path_len < 0 || isPathExpired(sender_path_len, sender_path_learned_at)) {
    if (packet->isRouteFlood() && packet->path_len > 0) {
        // Create PATH_RETURN to send our path back to sender
        mesh::Packet* pathPkt = createPathReturn(
            sender_hash,
            sender_secret,
            packet->path,  // The path the message took to reach us
            packet->path_len,
            0, NULL, 0  // No extra data
        );
        sendFlood(pathPkt);  // Send path info back
        Serial.printf("[ROUTE] Sent PATH_RETURN to sender (%d hops)\n", packet->path_len);
    }
}
```

**Rationale**: Explicit path exchange is more reliable than depending on ACKs.

### Fix #4: Increase Path Expiry Time

**File**: `src/config.h` or wherever PATH_EXPIRY_MS is defined

**Current**: (unknown)

**Proposed**:
```cpp
#define PATH_EXPIRY_MS (5 * 60 * 1000)  // 5 minutes
```

**Rationale**: Paths don't change that often in most use cases. 5 minutes allows multiple messages using same path.

### Fix #5: Add Path Retry Logic

**In `sendDirectMessage()` after direct send fails**:
```cpp
// If direct route fails (no ACK received), try flood on retry
if (attempts > 1 && lastAttemptWasDirect) {
    Serial.println("[DM] Direct route failed, retrying with FLOOD");
    sendFlood(pkt);  // Fallback to flood
    useDirect = false;
}
```

**Rationale**: Graceful degradation if path becomes invalid.

---

## 📊 Testing Plan

### Phase 1: Two-Device Test (Direct)
1. Device A and B in direct range
2. Send DM from A → B
3. **Verify**: Message received
4. **Check logs**: Should show "0 hops" (direct neighbor)

### Phase 2: Three-Device Test (One Hop)
1. Device A, Repeater R, Device B
2. A and B out of range, both in range of R
3. R has forwarding enabled: `mesh.setForwardingEnabled(true)`
4. Send DM from A → B
5. **Verify**:
   - First message uses FLOOD
   - ACK received with path_len=1
   - Path learned
   - Second message uses DIRECT route
   - Repeater forwards packet
   - Message received at B

### Phase 3: Four-Device Test (Two Hops)
1. Device A, Repeater R1, Repeater R2, Device B
2. Chain: A ↔ R1 ↔ R2 ↔ B
3. Both repeaters have forwarding enabled
4. Send DM from A → B
5. **Verify**:
   - Path learned with path_len=2
   - Direct routing through both hops
   - Both repeaters forward

### Phase 4: Stress Test
1. Send 10 DMs in rapid succession
2. **Verify**: All delivered, no packet loss
3. Check for forwarding loops (shouldn't happen with hasSeen table)

---

## 🎯 Implementation Priority

**Critical (Do First)**:
1. ✅ Fix #1: Allow direct packet forwarding
2. ✅ Fix #2: Learn paths from all ACKs
3. ✅ Verify forwarding is enabled on intermediate nodes
4. ✅ Add debug logging

**Important (Do Second)**:
1. Fix #3: Implement PATH_RETURN (more reliable)
2. Fix #4: Increase path expiry time
3. Add retry logic with fallback to flood

**Nice to Have**:
1. Path quality metrics (RSSI/SNR per hop)
2. Automatic path re-discovery
3. Multiple path support (path diversity)

---

## 📝 Summary

**Your implementation is fundamentally sound!** You're already using MeshCore's `sendDirect()` correctly.

**The likely culprits**:
1. **Forwarding not enabled** on intermediate nodes
2. **Paths only learned from flood ACKs**, not direct ACKs
3. **Potentially too aggressive path expiry**

**The fixes are small and targeted.** Most can be done in under 50 lines of code total.

**Next step**: Apply Fix #1 and #2, test with 3-device setup, and use debug logs to verify packet flow.

---

**Good luck!** The code structure is already excellent. These tweaks should make multi-hop DMs rock solid. 🚀
