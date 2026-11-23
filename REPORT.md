# EDControlPanel — Complete Project Audit Report (Updated)

**Date:** 2026-11-23
**Projects audited:** EDIpcProtocol, MasterController, SlaveController1, Server
**Scope:** All source code, configuration files, data assets, and build system files

---

## Executive Summary

The EDControlPanel project is an Elite Dangerous flight simulator hardware controller consisting of three embedded subprojects communicating via I2C (ESP32 master ↔ ATmega32U4 slave) and a C# server application. The codebase contains **18 critical bugs**, **33 high-severity issues**, and numerous medium/low findings across all subprojects.

### Architecture Note
**EDIpcProtocol is a single shared library** symlinked from both `MasterController/lib/EDIpcProtocol` and `SlaveController1/lib/EDIpcProtocol` (both are symlinks to `../../EDIpcProtocol`). It uses `#ifdef ESP32` / `#ifndef ESP32` preprocessor branches to compile master-side (ESP32) or slave-side (ATmega32U4) code from the same source files. Bugs in EDIpcProtocol affect whichever subproject compiles them.

### Severity Distribution

| Severity | EDIpcProtocol | MasterController | SlaveController1 | Config/Data | Total |
|-----------|--------------|-------------------|-------------------|-------------|-------|
| Critical  | 14           | 3                 | —                 | 1           | 18    |
| High      | 17           | 14                | —                 | 2           | 33    |
| Medium    | 15           | 10                | —                 | 5           | 30    |
| Low       | 5            | 8                 | —                 | 4           | 17    |

---

## 1. EDIpcProtocol Subproject (Shared Arduino Library)

**Location:** `EDIpcProtocol/`
**Purpose:** Inter-Process Communication library for ESP32 ↔ ATmega32U4 I2C communication, game state parsing, and HID simulation.
**Compilation:** `#ifdef ESP32` → master-side code (EDIpcProtocolMaster.cpp); `#ifndef ESP32` → slave-side code (EDIpcProtocolSlave.cpp, EDSerialProtocol.cpp)

### 1.1 CRITICAL Bugs

| # | File:Line | Subproject | Issue |
|---|-----------|------------|-------|
| 1 | `EDIpcProtocolSlave.cpp:73` | Slave | **RZ axis copy-paste bug:** `_joystick->setRzAxis(_axis.ry)` sets RZ to the Y-axis value instead of `_axis.rz`. The RZ axis always reports identically to RY. |
| 2 | `I2CDevice.cpp:240-252` | Master | **Unsigned underflow → buffer overflow:** `receivedDataSize = expectedSize - index` can underflow to a huge `uint8_t` value when `expectedSize < index`, then the `memcpy` at line 252 writes far beyond the buffer. |
| 3 | `I2CDevice.cpp:229-230` | Master | **Wrong memcpy size and return value:** Copies `receiveBufferSize` bytes from `chunkBuffer` but only `receivedSize` bytes are valid. Returns `receiveBufferSize` instead of actual bytes copied, causing callers to read uninitialized memory. |
| 4 | `EDSerialProtocol.cpp:86-142` | Slave | **strncpy without null termination:** `strncpy(res, buffer, maxsize)` where `maxsize=20` and destination is `char[21]`. If the source line is ≥20 bytes, no null terminator is written. |
| 5 | `EDIpcProtocolMaster.cpp:117-136` | Master | **Out-of-bounds read in `getDataFromBuffer()`:** The separator search loop advances `pos` past buffer end with no bounds check when the separator is never found. |
| 6 | `ThreadSafeWebSerial.cpp:18-20` | Master | **Race condition on semaphore:** `xSemaphoreCreateBinary()` creates a binary semaphore in the "not ready" (taken) state. If any `Lock()` call races before `begin()` completes, `_lock` is uninitialized → null pointer dereference. |
| 7 | `IAbstractDevice.h` | Master | **Missing virtual destructor:** Class has virtual methods but no `virtual ~IAbstractDevice() = default;`. Deleting through a base pointer is undefined behavior. |
| 8 | `EDIpcProtocolSlave.cpp:389-413` | Slave | **Undefined behavior software reset:** `void(*resetFunc)(void) = 0; resetFunc();` — calling address 0 as a function is UB. Works on AVR but catastrophic on ESP32 where `resetFunc` is never assigned. |
| 9 | `I2CDevice.cpp:281` | Slave | **Last-chunk boundary bug:** `responseSize % I2C_CHUNK_SIZE` returns 0 when `responseSize` is evenly divisible by `I2C_CHUNK_SIZE`. The last chunk sends 0 bytes, so the master never receives the final portion of data. E.g., a 32-byte response (exactly 2 chunks of 16) sends 0 bytes for chunk 2. |
| 10 | `EDIpcProtocolSlave.cpp:320` | Slave | **Wrong update flag cleared in CRT_GET_SYSTEM_POLICY:** Clears `UC_LOCATION` instead of `UC_SYSTEM_POLICY`. Copy-paste error from CRT_GET_LOCATION handler at line 311. |
| 11 | `EDIpcProtocolMaster.cpp:117-136` | Master | **Infinite loop in `getDataFromBuffer()`:** When the separator is never found in the source buffer, `pos` increments past the buffer boundary and the loop never terminates. Same root cause as #5 but specifically: the separator search loop at line 132-133 has no bounds check. |
| 12 | `EDIpcProtocolSlave.cpp:21-25` | Slave | **Missing destructor for `_joystick`:** `EDIpcProtocolSlave` allocates `Joystick_` with `new` at construction but has no destructor to `delete` it. Rule of Three violation — memory leak on program termination. |
| 13 | `ThreadSafeWebSerial.cpp:8` | Master | **Semaphore creation failure not checked:** If `xSemaphoreCreateBinary()` returns NULL (e.g., insufficient heap), all subsequent `Lock()`/`Unlock()` calls will crash with null pointer dereference. |
| 14 | `EDIpcProtocolMaster.cpp:22-27` | Master | **Queue creation failure has no fallback:** If `xQueueCreateStatic()` fails, a `Serial.println` prints but key events are permanently lost with no error recovery mechanism. |

### 1.2 High Severity Issues

| # | File:Line | Subproject | Issue |
|---|-----------|------------|-------|
| 11 | `EDIpcProtocolMaster.cpp:141-158,161-172,234-249` | Master | **Always returns false:** `_getGameStatus()`, `_getLoadoutStatus()`, and `_getUrgentInfosData()` all unconditionally `return false;` regardless of whether data was successfully received. In `retrieveChanges()`, `result &= _getGameStatus()` always sets result to false, masking actual data reception success. |
| 12 | `I2CDevice.cpp:14,189` | Master | **Global mutable state:** `CRC8 crc;` and `uint8_t chunkBuffer[I2C_MAX_PACKET_SIZE];` are module-level globals. Not thread-safe on ESP32 (multi-core). |
| 13 | `EDIpcProtocolSlave.cpp:39,40` | Slave | **ISR-blocking I2C handler:** `onReceive` and `onRequest` ISR callbacks call `_handleRequest()` which performs CRC computation and buffer preparation — significant work inside an ISR context. |
| 14 | `I2CDevice.cpp:58` | Master | **Invalid address sentinel:** `Addr = -1;` assigns -1 to `uint8_t`, wrapping to 255. Not a valid I2C address but used as "not found" sentinel. |
| 15 | `EDIpcProtocolSlave.cpp:96` | Slave | **Blocking delay in signal path:** `delay(10)` inside `signalMaster()` blocks the entire loop for 10ms on every signal. |
| 16 | `EDIpcProtocolMaster.cpp:62-101,254-257` | Master | **Dead code:** Large commented-out blocks (`getComMcuData()`, axis data sending) should be removed. |
| 17 | `EDIpcProtocolSlave.cpp:94-98` | Slave | **Blocking delay in signal path:** `signalMaster()` uses `delay(10)` which blocks the entire loop. |
| 18 | `I2CDevice.cpp:127-168` | Master | **Non-reentrant requestData():** Uses module-level `chunkBuffer` and `CRC8 crc`. Concurrent calls from multiple tasks corrupt state. |
| 19 | `DeviceManager.cpp:43-44` | Master | **Detection result silently ignored:** `bool detected = Devices[i]->Detect(...)` result is assigned but never checked. A device that fails detection still proceeds to `Init()`. |
| 20 | `EDIpcProtocolSlave.cpp:126-132` | Slave | **Blocking `delay(50)` in main processing loop:** `_processKeyEventQueue` calls `delay(50)` per key event. For `count > 1` keys, this blocks the entire main loop for 100ms+ per cycle. |
| 21 | `EDIpcProtocolSlave.cpp:156-158` | Slave | **Queue full silently drops events with no logging:** When the key event queue is full, events are dropped silently — no notification to caller, no logging. |
| 22 | `EDIpcProtocolMaster.cpp:103-108` | Master | **`_sendAxisData()` always returns `false`:** The function always returns `false`, making its return value useless and any caller checking it (line 254-257) always skip axis data sending. |
| 23 | `ThreadSafeWebSerial.h:17-18` | Master | **Public `Lock()`/`Unlock()` methods:** External code can acquire the lock and never release it, causing permanent deadlock. Should be private with RAII wrapper (e.g., `std::lock_guard`-style class). |
| 24 | `Logger.h:10, Logger.cpp:15-16` | Master | **Data race on `_logfunc`:** `SetLogger()` writes `_logfunc` without synchronization. `Log()` reads it without synchronization. A concurrent `SetLogger` call during `Log()` execution is a data race. |
| 25 | `I2CDevice.cpp:256,266` | Master | **Hardcoded `delay(5)` / `delay(10)` in receive loop:** Fixed delays in the I2C receive loop make the protocol slow and non-configurable. |
| 26 | `EDIpcProtocolSlave.cpp:34` | Slave | **Hardcoded I2C address 0x12:** The `begin(0x12)` call in `reset()` uses a hardcoded address. Should be configurable via define or parameter. |
| 27 | `EDSerialProtocol.cpp:5` | Slave | **Hardcoded version string:** `"EDControlPanel V1.0.1"` is hardcoded in SRAM. Should be configurable via build flag and stored in PROGMEM. |

### 1.3 Protocol Mismatches (Serial vs I2C)

The EDIpcProtocol library has two communication paths: **Serial** (ATmega ↔ external PC/software via `EDSerialProtocol.cpp`) and **I2C** (ESP32 ↔ ATmega via `EDIpcProtocolSlave.cpp`/`EDIpcProtocolMaster.cpp`). These paths have inconsistent field layouts:

| Command | Serial Protocol (EDSerialProtocol.cpp) | I2C Protocol (Slave→Master) | Mismatch |
|---------|----------------------------------------|-----------------------------|----------|
| `LOCATION` | 4 fields: SystemName, StationName, Allegiance, Security (lines 86-89) | 2 fields: SystemName, StationName (slave:306-308) | Allegiance+Security sent separately via `CRT_GET_SYSTEM_POLICY` (slave:315-317) |
| `LOADOUT` | 3 fields: Flags1 (4B), Flags2 (4B), **unused string** (line 147: `ReadLine(nullptr, 20, '\0')`) | 2 fields: Flags1 (4B), Flags2 (4B) (slave:369-370) | Serial reads a 3rd null-terminated string field that I2C never sends. Data discarded (`nullptr`), so no functional impact. |

The `CRT_GET_SYSTEM_POLICY` command (slave:312-320) was likely added to split the 4-field LOCATION command into two 2-field commands, but the serial protocol was never updated to match.

### 1.4 Medium / Low Issues

| # | File | Issue |
|---|------|-------|
| 17 | `README.md`, `keywords.txt` | **Copied from wrong library:** Both files are verbatim copies of `LiquidCrystal_I2C` by enjoyneering. Zero EDIpcProtocol-specific content. |
| 18 | `ThreadSafeWebSerial.cpp:66` | **Memory leak:** Binary semaphore `_lock` is never deleted (no `vSemaphoreDelete`). |
| 19 | `doc/serialProtocol.md:103` | **Protocol header collision:** Header char `V` used for both "Verbose" command and "Location" message. |
| 20 | `EDSerialProtocol.cpp:121-124` | **Endianness-dependent binary protocol:** Raw `Serial.readBytes()` reads binary data directly — fragile across architectures. |
| 21 | `I2CDevice.h:36` | **Fragile interrupt pin type:** `digitalPinToInterrupt()` result stored in `uint8_t`, but the function can return -1 or an enum value. |
| 22 | `EDIpcProtocolSlave.cpp:422-442` | **Dead code:** Empty if-blocks with only commented-out Serial debug prints. |
| 23 | `EDSerialProtocol.cpp:30` | **Unprofessional comment:** "Arduino is a pain in the ass with switch statements" — inaccurate for modern avr-gcc. |
| 24 | `EDSerialProtocol.cpp:5` | **`infoString` in RAM instead of PROGMEM:** Wastes precious SRAM on ATmega32U4. |
| 25 | `EDGameVariables.cpp:218-266` | **Loadout flag checks use `> 0`** instead of proper bitwise masking. Works for these flags but is semantically incorrect. |
| 26 | `EDIpcProtocolMaster.h:33` | **`_getWifiCredentials` declared but never implemented:** Method declared in header, no corresponding `.cpp` implementation. Dead code. |
| 27 | `EDGameVariables.h:186-187` | **LoadoutFlags initialized to `0xFFFFFFFF`:** All equipment "enabled" by default before first I2C update. Should be `0x00000000` to represent "disabled" as default state. |
| 28 | `I2CDevice.h:27` | **Overloaded `getData` with too many optional parameters (6 params, 4 optional):** Poor API design. Should use a struct or builder pattern for clarity. |
| 29 | `DeviceManager.cpp:14-36` | **I2C scanning code in production:** Two scanning loops (Wire and Wire1) that print all detected devices to Serial should be guarded by a debug macro or removed. |
| 30 | `EDIpcProtocolMaster.cpp:41` | **Debug `Serial.println("Key queued")` in production code:** Should be guarded by a debug macro. |
| 31 | `EDIpcProtocolMaster.cpp:156` | **Raw buffer debug print:** `TSWebSerial.println(receiveBuffer+8)` prints raw bytes from the buffer. If the buffer contains non-printable or binary data, this can produce garbage output or crash. |
| 32 | `EDIpcProtocol.h:33` | **`CRT_GET_STAR_SYSTEM` commented out at value 0:** Leaves value 0 unused. If a future request uses value 0, it would conflict with `CRT_NONE`. |
| 33 | `EDIpcProtocol.h:46` | **`CRT_MOCK` at value 254:** Value 255 is `CRT_GET_PING_SLAVE`. No gap for future expansion near the boundary. |
| 34 | `I2CDevice.cpp:209-210` | **Ambiguous return value:** `sendMessageData` failure returns 0, which is the same as "no data". Cannot distinguish failure from empty response. |

---

## 2. MasterController Subproject (ESP32 — Wemos D1 Mini32)

**Location:** `MasterController/`
**Purpose:** Main controller with LCD display, keypad, LED indicators, rotary encoders, WiFi web management, and I2C master communication to SlaveController1.

### 2.1 CRITICAL Bugs

| # | File:Line | Issue |
|---|-----------|-------|
| 1 | `src/ActionMapper.cpp:428` | **Missing format argument in printf:** `TSWebSerial.printf("Hardpoint %d found\n");` — `%d` has no corresponding argument. Reads garbage from stack → undefined behavior / potential crash. |
| 2 | `src/main.cpp:569` | **Raw binary data treated as string:** `Serial.printf("Got %s\n", buf);` where `buf` is raw binary I2C data, not null-terminated. Can read past buffer bounds. |
| 3 | `src/main.cpp:422-426` | **Infinite blocking boot loop:** `while (!comMcu->Detect(...))` has no timeout. If slave MCU is never detected, device hangs permanently at boot. |

### 2.2 High Severity Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 4 | `src/main.cpp:64-66` | **Memory leak:** Three global `RotaryEncoder` objects allocated with `new`, never freed. |
| 5 | `src/EDDisplayManager.cpp:21-27` + header | **Memory leak:** 7 DisplayPage objects allocated with `new`, no destructor to free them (~588 bytes total). |
| 6 | `src/I2CDeviceDisplay.cpp:26` | **Memory leak:** LCD controller allocated with `new`, never freed. |
| 7 | `src/LedController.cpp:12` | **Memory leak:** `_tpic` allocated with `new`, never freed. |
| 8 | `src/I2CDeviceKeyController.cpp:48` | **Memory leak:** `IOExpander` (PCF8575) allocated with `new`, never freed. |
| 9 | `src/EDControllerWebServer.cpp:15` | **Memory leak:** `_server` (AsyncWebServer) allocated with `new`, never freed. |
| 10 | `include/Macro.h:118` + `src/Macro.cpp:35-45` | **Rule of Three violation:** MacroStep objects stored as raw pointers in `std::vector<Macro*>`. Copy constructor/assignment do shallow copies → double-free risk. |
| 11 | `src/I2CDeviceComMcu.cpp:58-75` | **Non-reentrant globals:** `COM_REQUEST_TYPE mesId`, `responseBuffer[]`, `CRC8 sendcrc` are module-level globals used in `requestData()`. Concurrent calls from different tasks corrupt state. |
| 12 | `src/ActionMapper.cpp:406-414,372-385` | **Unsynchronized map access:** `_toggles` and `_inconsistentToggles` maps iterated from multiple RTOS tasks (core 0 and core 1) without mutex protection. |
| 13 | `src/EDControllerWebServer.cpp:129-166` | **HTTP handler calls ESP.restart():** `LittleFS.end()` + `ESP.restart()` called synchronously in async web handler — HTTP response may not send before restart. |
| 14 | `src/ActionMapper.cpp:343` | **Unchecked map access:** `_actionsMap[item]` inserts a default-constructed (zeroed) entry if key doesn't exist, silently proceeding with invalid config. |
| 15 | `src/main.cpp:351-364` | **Raw byte values written to LCD buffer:** Arbitrary values 0-255 written directly into display buffer — could corrupt LCD display. Appears to be debug/test code left in production. |
| 16 | `src/ActionMapper.cpp:179` | **Zero key codes for button action:** `Button` type with `pressedKey = 0x00, releasedKey = 0x00` — sends key code 0 which could be interpreted as a real key. |
| 17 | `src/EDDisplayManager.cpp:92-96` | **Blocking delay in scroll mode:** `delay(_scrollDelay)` in `I2CDeviceDisplay::PrintAt()` blocks the entire task during scroll animation. |
| 18 | `src/EDDisplayManager.cpp:78-86,161-169` | **Duplicate CRC calculation code** — should be a helper function. |

### 2.3 Security Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 19 | `src/EDControllerWebServer.cpp:168-190` | **No authentication on any web endpoint:** All configuration endpoints (`/savewifi`, `/savemapping`, `/savehardpoints`, `/savemacros`, `/clearwifi`) accessible without auth. |
| 20 | `src/EDControllerWebServer.cpp:170-190` | **Plaintext WiFi credentials:** Passwords stored unencrypted on LittleFS filesystem. |
| 21 | `src/EDControllerWebServer.cpp:209-218` | **No input validation on HTTP parameters:** `fire_group = 0` → `fireGroup = 255` (unsigned wraparound). No bounds checking. |
| 22 | `data/html/wifi.html:53` | **Credentials sent over unencrypted HTTP:** WiFi credentials transmitted in cleartext. |
| 23 | `data/html/wifi.html:50` | **XSS vulnerability:** `%WIFI_SSID%` template variable directly interpolated into HTML without escaping. |

### 2.4 Thread Safety / Concurrency Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 24 | `src/I2CDeviceComMcu.cpp:10-16` | **Non-atomic volatile bool:** `_hasComMcuInterrupt` is `volatile bool` — no atomic guarantee on ESP32. Should use `std::atomic<bool>`. |
| 25 | `src/main.cpp:181-253` | **String allocation in RTOS task:** Continuous `String` allocation/deallocation in TaskKeypadCode can fragment ESP32 heap. |
| 26 | `src/EDDisplayManager.cpp:61-69` | **Binary semaphore used as mutex:** Binary semaphores lack priority inheritance. Should use `xSemaphoreCreateMutex()`. Risk of deadlock if exception/early return occurs between Lock and Unlock. |
| 27 | `src/main.cpp:626-639` | **Manual throttling instead of RTOS:** `AsyncWebServer.loop()` throttled to every 50ms — TCP handling should be as frequent as possible. |

### 2.5 PlatformIO / Build Issues

| # | File | Issue |
|---|------|-------|
| 28 | `platformio.ini:29-30` | **Hardcoded Windows COM ports:** `monitor_port = COM6`, `upload_port = COM6` — breaks on Linux/macOS. |
| 29 | `platformio.ini:32` | **Debug build type:** `build_type = debug` should be `release` for production firmware. |
| 30 | `platformio.ini:12` | **No platform version pinning:** Platform pulls latest version, which may introduce breaking changes. |
| 31 | `platformio.ini:24` | **Forked dependency with branch ref:** `WebSerial.git#makeBuffer` — fragile, should pin to commit hash. |
| 32 | `platformio.ini:26` | **ArduinoJson version conflict:** Project uses ArduinoJson v7.1.0 but ESPAsyncWebServer bundles v6 — potential compilation errors. |
| 33 | `partition_layout.csv` | **SPIFFS partition allocated but unused:** 512KB wasted. Config uses LittleFS but partition table defines `spiffs`. |
| 34 | `partition_layout.csv` | **No factory OTA fallback:** Only `ota_0` and `ota_1` defined. If both slots corrupt, no recovery path. |
| 35 | `platformio.ini:18-20` | **Local libraries with example files:** `Keypad8575/` and `LiquidCrystal_I2C/` include `examples/` subdirectories — `.ino` files may cause compilation conflicts. |

### 2.6 Incomplete Implementations / Dead Code

| # | File:Line | Issue |
|---|-----------|-------|
| 36 | `src/main.cpp:104-111` | **TODO comments:** DSD and ACS scanner activation not implemented. |
| 37 | `src/Macro.cpp:189` | **TODO:** Delay step uses `vTaskDelay()` which freezes the entire task during delay. |
| 38 | `src/ActionMapper.cpp:33-35,73-74` | **Unused methods:** `LoadMapping()` and `SaveMapping()` marked "unused at the moment". |
| 39 | `include/OTAUpdate.h.bak` | **Stale backup file committed to repo** with hardcoded credential placeholders. |
| 40 | `src/main.cpp:549-560` | **Commented-out display commands:** Dead code for never-implemented display features. |
| 41 | `src/I2CDeviceComMcu.cpp:8` | **Commented-out OTA update code** referencing non-functional class. |
| 42 | `src/ActionMapper.cpp:339` | **Debug Serial.print in production code:** Prints on every action trigger. |
| 43 | `src/ActionMapper.cpp:412` | **Debug Serial.print in production code.** |
| 44 | `src/ActionMapper.cpp:379` | **Missing newline in printf:** `Serial.printf("Key %d inconsistent...` — no `\n` terminator. |

### 2.7 Code Quality Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 45 | `src/main.cpp:13-14` | **Duplicate include:** `<LedController.h>` included twice. |
| 46 | `src/LedController.cpp:132-143` | **Blinking logic bug:** `(millis() - lastUpdate) % 1000 < 500` with guard `> 500` means first execution always has `alternate = false`. |
| 47 | `src/ActionMapper.cpp:493-507` | **Timestamp wrap-around:** `millis()` overflows after ~49.7 days; comparison logic doesn't handle wrap-around. |
| 48 | `src/main.cpp:26-27` | **Broken debug macro:** `#ifdef DEBUG` may not match PlatformIO's `CORE_DEBUG_LEVEL` define. No code in `#else` branch. |

---

## 3. SlaveController1 Subproject (ATmega32U4 — SparkFun Pro Micro)

**Location:** `SlaveController1/`
**Purpose:** I2C slave device that reads game state from ESP32 master, drives virtual USB Joystick + Keyboard HID, and exposes serial command interface.

**Note:** SlaveController1 contains only `src/main.cpp` (55 lines) plus a symlink to the shared EDIpcProtocol library. All slave-side bugs are in the EDIpcProtocol source files (see Section 1).

### 3.1 Slave-Specific Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 1 | `src/main.cpp:34-35` | **Memory leak:** `IpcProtocolInstance` and `SerialProtocolInstance` allocated with `new`, never freed. On ATmega32U4 (2.5KB SRAM), heap fragmentation is a real concern. |
| 2 | `src/main.cpp:16-21` | **`GetFreeRam()` implementation:** Uses heap pointer arithmetic — unreliable on all architectures, only works as an approximation. |

### 3.2 Medium / Low Issues

| # | File:Line | Issue |
|---|-----------|-------|
| 3 | `platformio.ini:17-18` | **Hardcoded Windows COM port:** `monitor_port = COM12`, `upload_port = COM12`. |
| 4 | `EDIpcProtocolSlave.cpp:10-13,105-140` | **Key event queue issues:** Raw `KeyEvent*` pointers with `new`/`delete`. Queue wrap-around leaks old entries. `delete` happens outside ISR critical section. |
| 5 | `EDIpcProtocolSlave.cpp:408` | **Unchecked `readBytes()` return value:** Key data reception never validates byte count. Partial reads result in garbage `KeyEvent` queued. |
| 6 | `EDIpcProtocolSlave.cpp:394` | **Unchecked `Wire.read()`:** Before reading `requestVal`, availability is not checked. Returns -1 (cast to large uint8_t) if no data available. |
| 7 | `EDIpcProtocolSlave.cpp:373` | **Wrong update flag cleared:** `CRT_GET_LOADOUT` clears `UC_STATUS` instead of `UC_LOADOUT` — copy-paste error from line 365. |
| 8 | `EDIpcProtocol/EDSerialProtocol.cpp:25-33` | **Static buffer not reentrant:** `ReadLine()` uses static `char buffer[101]`. Recursive or interrupt calls corrupt the buffer. |
| 9 | `EDIpcProtocol/EDSerialProtocol.cpp:121-124,145,176` | **Unchecked `Serial.readBytes()`:** Return values never checked — partial reads leave destination variables partially uninitialized. |
| 10 | `EDIpcProtocol/EDGameVariables.h` | **~322 bytes SRAM consumption:** 14 string fields of 21 bytes each = ~13% of total 2.5KB SRAM in a single global object. |
| 11 | `EDIpcProtocolSlave.cpp:173-183` | **Inconsistent buffer overflow checks:** `_writeTxBuffer(const char*)` uses `>` vs `_writeTxBuffer(const __FlashStringHelper*)` uses `>=` — inconsistent boundary behavior. |

---

## 4. Configuration, Data & Build System

### 4.1 CRITICAL

| # | File | Issue |
|---|------|-------|
| 1 | `MasterController/data/html/mapping.html:390` | **Save button broken:** `save;` instead of `save();` — clicking "Save changes" does nothing (just evaluates the function reference, no call). |
| 2 | `MasterController/.vscode/c_cpp_properties.json` | **399 lines of hardcoded Windows paths** (`C:/Users/jlongo/...`). Completely non-portable to other machines. |
| 3 | `MasterController/data_uncompressed/html/` | **Empty directory** — data integrity issue. Should either be populated to match `data/` or removed. |

### 4.2 High

| # | File | Issue |
|---|------|-------|
| 4 | Both `platformio.ini` files | **Hardcoded Windows COM ports** — non-portable across OSes. |
| 5 | `Server/EliteFIPServer - Copie/` | **Duplicate directory** (French "Copie" = "Copy") containing build artifacts (`.vs/`, `obj/`). Wastes disk space and creates confusion. |
| 6 | Both `platformio.ini` | **No platform version pinning** — latest platform pulled on every build. |
| 7 | `MasterController/platformio.ini:18-20` | **Local libraries include example `.ino` files** (`Keypad8575/examples/`, `LiquidCrystal_I2C/examples/`) that may conflict during compilation. |

### 4.3 Medium

| # | File | Issue |
|---|------|-------|
| 8 | Both `test/` directories | **No unit tests** — only default PlatformIO README placeholder. |
| 9 | `MasterController/platformio.ini:32` | **Debug build type** with no release alternative environment. |
| 10 | `MasterController/README.md` (root) | **Single-line README** (`# EDControlPanel`) — no build instructions, hardware requirements, or architecture overview. |
| 11 | `MasterController/include/OTAUpdate.h.bak` | **Stale backup file** with credential placeholders committed to repo. |
| 12 | `Server/EliteFIPServer/package.json` | **Incorrect `main` field** — says `"index.js"` for a .NET/C# project. |
| 13 | `MasterController/partition_layout.csv` | **SPIFFS partition allocated but unused** (512KB wasted), no factory OTA fallback partition. |

### 4.4 Low

| # | File | Issue |
|---|------|-------|
| 14 | `MasterController/MasterController.code-workspace` | Duplicates `.vscode/settings.json` content. |
| 15 | Root level | Binary files (`buttons.pdf`, `buttons.svg`, `implantation.svg`) in root instead of `docs/` directory. |
| 16 | Both `.gitignore` | Could be more comprehensive for PlatformIO build artifacts (`*.bin`, `*.elf`, `*.hex`). |
| 17 | `SlaveController1/.vscode/c_cpp_properties.json` | Hardcoded Windows paths. |

---

## 5. Cross-Cutting Architecture Issues

### 5.1 Global Singleton Proliferation
Every subproject uses global singletons: `Logger`, `EDGameVariables`, `EDIpcProtocolSlave::instance`, `EDIpcProtocolMaster::instance`, `ActionMapperClass`, `LedController`, `EDControllerWebServer`. This makes testing impossible and creates hidden coupling.

### 5.2 Platform Split Confusion
The EDIpcProtocol library serves both ESP32 (master) and non-ESP32 (slave) roles via `#ifdef ESP32` preprocessor branches. The same source files compile for two completely different architectures, limiting independent testing.

### 5.3 No Error Propagation
Most functions return `bool` or `void` with no detailed error info. Callers cannot distinguish between "device not found", "communication timeout", or "CRC mismatch".

### 5.4 No Input Validation
Serial commands, HTTP endpoints, and I2C messages accept arbitrary input without validation or authentication. The reboot command (`CRT_SEND_REBOOT`) has no authorization check — any I2C master can reset the slave.

### 5.5 Dead Code Accumulation
Commented-out code blocks scattered throughout all three subprojects suggest abandoned features that were never cleaned up, adding confusion and maintenance burden.

### 5.6 Shared Library Symlink Pattern
Both `MasterController/lib/EDIpcProtocol` and `SlaveController1/lib/EDIpcProtocol` are symlinks to `../../EDIpcProtocol`. This is correct architecture (single source of truth), but the platformio.ini files should document this relationship.

---

## 6. Priority Remediation Plan

### Phase 1: Critical (Fix Immediately)
1. **Fix RZ axis bug** — `EDIpcProtocolSlave.cpp:73`: Change `_axis.ry` to `_axis.rz`
2. **Fix printf format bug** — `ActionMapper.cpp:428`: Add missing `%d` argument
3. **Fix buffer overflow in I2CDevice** — `I2CDevice.cpp:240-252`: Add bounds checking and fix unsigned underflow
4. **Fix strncpy null termination** — `EDSerialProtocol.cpp`: Ensure null termination after `strncpy`
5. **Fix getDataFromBuffer OOB read** — `EDIpcProtocolMaster.cpp:117-136`: Add buffer bounds check
6. **Add timeout to slave detection loop** — `main.cpp:422`: Replace infinite while with timeout-based loop
7. **Fix mapping.html save button** — Change `save;` to `save();`
8. **Fix UB software reset** — `EDIpcProtocolSlave.cpp:389`: Use proper watchdog reset or `while(1)`
9. **Fix missing virtual destructor** — `IAbstractDevice.h`: Add `virtual ~IAbstractDevice() = default;`
10. **Fix semaphore race condition** — `ThreadSafeWebSerial.cpp`: Initialize semaphore before use
11. **Fix last-chunk boundary bug** — `I2CDevice.cpp:281`: Use `(responseSize % I2C_CHUNK_SIZE == 0 && chunkId == chunkCount - 1) ? I2C_CHUNK_SIZE : responseSize % I2C_CHUNK_SIZE` or equivalent
12. **Fix wrong update flag in CRT_GET_SYSTEM_POLICY** — `EDIpcProtocolSlave.cpp:320`: Change `UC_LOCATION` to `UC_SYSTEM_POLICY`

### Phase 2: High (Fix Before Release)
13. Add I2C mutex protection (uncomment and fix `i2cMutex`)
14. Add input validation to all web endpoints
15. Add authentication to configuration endpoints
16. Fix all memory leaks (add destructors or use RAII/static objects)
17. Protect shared data structures with proper synchronization (mutexes, atomics)
18. Remove hardcoded COM ports from platformio.ini files
19. Fix always-false return values — `_getGameStatus()`, `_getLoadoutStatus()`, `_getUrgentInfosData()` should return actual success status
20. Remove dead code and commented-out blocks from all subprojects
21. Fix detection result ignored in `DeviceManager.cpp:43-44`
22. Remove blocking delays from ISR context and signal paths
23. Fix protocol mismatch between serial and I2C for LOCATION/LOADOUT commands

### Phase 3: Medium (Improve Reliability)
24. Pin platform versions in platformio.ini files
25. Add release build configuration
26. Fix SPIFFS partition → define LittleFS partition or remove unused one
27. Add unit tests for protocol parsing and I2C communication
28. Fix XSS vulnerability in wifi.html template interpolation
29. Move binary asset files to `docs/` directory
30. Delete `Server/EliteFIPServer - Copie/` duplicate
31. Delete `OTAUpdate.h.bak` backup file
32. Remove example `.ino` files from local library copies
33. Replace `volatile bool` with `std::atomic<bool>` for shared flags
34. Fix LoadoutFlags initialization — change `0xFFFFFFFF` to `0x00000000` in `EDGameVariables.h:186-187`

### Phase 4: Low (Cleanup)
35. Fix README.md at project root with build instructions and architecture overview
36. Remove hardcoded Windows paths from c_cpp_properties.json (or add to .gitignore)
37. Clean up data_uncompressed/ directory
38. Replace `boolean` with `bool` in EDSerialProtocol
39. Move `infoString` to PROGMEM on ATmega32U4
40. Fix unprofessional comment about switch statements
41. Add newline to incomplete Serial.printf at `ActionMapper.cpp:379`
42. Update EDIpcProtocol README.md and keywords.txt with actual library content

---

## 7. Files Audited

### EDIpcProtocol (Shared Library)
- `library.json`, `library.properties`, `keywords.txt`, `README.md`
- `doc/serialProtocol.md`
- `src/ThreadSafeWebSerial.h/.cpp`
- `src/Logger.h/.cpp`
- `src/IAbstractDevice.h`
- `src/I2CDevice.h/.cpp`
- `src/EDIpcProtocol.h`
- `src/EDGameVariables.h/.cpp`
- `src/EDSerialProtocol.h/.cpp` (non-ESP32 path)
- `src/EDIpcProtocolSlave.h/.cpp` (non-ESP32 / slave path)
- `src/EDIpcProtocolMaster.h/.cpp` (ESP32 / master path)
- `src/DeviceManager.h/.cpp`

### MasterController (ESP32)
- `platformio.ini`, `partition_layout.csv`, `.gitignore`, `MasterController.code-workspace`
- `src/main.cpp`, `src/ActionMapper.cpp`, `src/EDDisplayManager.cpp`, `src/EDControllerWebServer.cpp`
- `src/LedController.cpp`, `src/RotaryEncoder.cpp`, `src/I2CDeviceDisplay.cpp`, `src/I2CDeviceKeyController.cpp`, `src/I2CDeviceComMcu.cpp`
- `src/Macro.cpp`, `src/ShiftRegisterTPIC6B595.cpp/.h`
- `include/EDDisplayManager.h`, `include/I2CDeviceDisplay.h`, `include/Macro.h`, `include/OTAUpdate.h.bak`
- `data/html/*.html`, `data/html/*.css`, `data/html/*.js`, `data/html/*.jpg`, `data/html/*.png`
- `lib/EDIpcProtocol/` (symlink → ../../EDIpcProtocol)
- `lib/Keypad8575/`, `lib/LiquidCrystal_I2C/`

### SlaveController1 (ATmega32U4)
- `platformio.ini`, `.gitignore`
- `src/main.cpp`
- `test/README` (empty — no tests)
- `lib/EDIpcProtocol/` (symlink → ../../EDIpcProtocol)

### Server
- `Server/EliteFIPServer/` and `Server/EliteFIPServer - Copie/` (duplicates)
- `package.json`, `README.md`

### Root
- `README.md`, `buttons.pdf`, `buttons.svg`, `implantation.svg`
