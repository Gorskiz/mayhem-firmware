# Pull Request Descriptions

Use the content below for your Pull Requests.

---

## 1. Weather Protocols
**Branch:** `feat/add-weather-protocols`

**Title:** feat: Add Fine Offset WH2, Ecowitt, and Bresser 5-in-1 Weather Protocols

**Body:**
### Description
This PR adds three new weather station protocols to the `fprotos` decoder system:
1. **Fine Offset WH2**: Very popular outdoor sensor protocol.
2. **Ecowitt**: Widely used in modern home weather stations.
3. **Bresser 5-in-1**: Complex multi-sensor packet decoding.

### Technical Details
- Implemented in `w-fineoffset_wh2.hpp`, `w-ecowitt.hpp`, and `w-bresser_5in1.hpp`.
- Follows existing `WeatherBase` pattern.
- Includes full CRC/Checksum validation for robust decoding.
- Added new enum types to `weathertypes.hpp`.

### Testing
- Validated compile status.
- Checked CRC logic against protocol specifications.

---

## 2. Enhanced Logging
**Branch:** `feat/enhanced-logging-timestamps`

**Title:** feat: Add GPS Coordinates to System Logs

**Body:**
### Description
Enhances the `LogFile` class to support optional GPS tagging for log entries. This is essential for wardriving, signal mapping, and field diagnostics.

### Technical Details
- Added `write_entry_with_gps()` overload to `log_file.cpp`.
- Formats entries as: `TIMESTAMP [GPS:LAT,LON] MESSAGE`.
- Lat/Lon are formatted to 6 decimal places for high precision.
- Fully backward compatible with existing logging calls.

### Testing
- Verified compilation.
- Checked string formatting logic.

---

## 3. SubGhz Protocols
**Branch:** `feat/add-subghz-protocols`

**Title:** feat: Add Ansonic and ELRO SubGhz Protocols

**Body:**
### Description
Adds support for two popular 433MHz protocols often requested by the community:
1. **Ansonic**: Used in doorbells and garage remotes.
2. **ELRO**: Common home automation and remote socket protocol.

### Technical Details
- Added `s-ansonic.hpp` and `s-elro.hpp`.
- Registered in `subghzdprotos.hpp`.
- Uses standard OOK/ASK demodulation patterns.

---

## 4. Button Combo Shortcuts
**Branch:** `feat/button-combo-shortcuts`

**Title:** feat: Add Global Button Combo Shortcuts (Screenshot, Home, Sleep)

**Body:**
### Description
Adds "Power User" shortcuts to improve navigation and utility without menu digging.

**New Shortcuts:**
- **Select + Right**: Take immediate **Screenshot** (saves to /SCREENSHOTS).
- **Select + Left**: Jump immediately to **Home Screen**.
- **Select + Down**: Toggle **Display Sleep** (Stealth Mode).

### Technical Details
- Modified `event_m0.cpp` to handle multi-key presses before single key dispatch.
- Added `KeyEvent::Screenshot`, `Home`, and `Sleep` internal events.
- Added public helper methods to `SystemView` to expose these actions safely.
- Retains existing `Left + Up` (Back) combo functionality.

---

## 5. FreqMan Empty DB Crash Fix
**Branch:** `fix/freqman-empty-database-crash`

**Title:** fix: Prevent Hard Fault when editing empty Frequency Manager DB

**Body:**
### Description
Fixes a critical bug/hard fault that occurred when attempting to edit or save a frequency list that was completely empty or had invalid pointers.

### Fix
- Added null pointer checks in `ui_freqman.cpp`.
- Ensure `entry_ptr` is valid before dereferencing in `on_save()`.

---

## 6. SD Format Utility
**Branch:** `feat/sd-format-utility`

**Title:** feat: Add SD Card Format Utility

**Body:**
### Description
Adds a standalone utility app to format SD cards directly from the PortaPack. Useful for recovering corrupted cards in the field without a PC.

### Features
- Formats card to FAT32.
- Sets correct cluster size automatically.
- Includes "Are you sure?" safety prompt.
- Located in Utilities menu.

### Technical Details
- Calls `sd_card::format()` low-level driver.
- Handles unmounting/remounting of filesystem.

---
