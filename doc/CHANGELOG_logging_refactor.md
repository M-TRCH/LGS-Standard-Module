# Changelog - Logging System Refactor

## Date: October 30, 2025

## Summary
Refactored the entire logging system to provide better structure, standardization, and flexibility across all modules in the project.

## Changes Made

### 1. New Logging System (`include/system.h`)

#### Added Log Levels
- `LOG_NONE` (0) - No logging
- `LOG_ERROR` (1) - Critical errors only
- `LOG_WARNING` (2) - Warnings and errors
- `LOG_INFO` (3) - Informational messages (default)
- `LOG_DEBUG` (4) - Debug information
- `LOG_VERBOSE` (5) - Verbose debug output

#### Added Log Categories (Bit Flags)
- `LOG_CAT_NONE` (0x00) - No categories
- `LOG_CAT_SYSTEM` (0x01) - System operations
- `LOG_CAT_EEPROM` (0x02) - EEPROM operations
- `LOG_CAT_MODBUS` (0x04) - Modbus communications
- `LOG_CAT_LED` (0x08) - LED control
- `LOG_CAT_ALL` (0xFF) - All categories (default)

#### New Global Configuration Variables
```cpp
extern LogLevel globalLogLevel;        // Controls overall log level
extern uint8_t enabledLogCategories;   // Controls which categories to show
```

#### New Logging Macros
**Generic Macro:**
```cpp
LOG(level, category, msg)
```

**Category-Specific Macros:**
```cpp
// System
LOG_ERROR_SYS(msg)
LOG_WARNING_SYS(msg)
LOG_INFO_SYS(msg)
LOG_DEBUG_SYS(msg)
LOG_VERBOSE_SYS(msg)

// EEPROM
LOG_ERROR_EEPROM(msg)
LOG_WARNING_EEPROM(msg)
LOG_INFO_EEPROM(msg)
LOG_DEBUG_EEPROM(msg)
LOG_VERBOSE_EEPROM(msg)

// Modbus
LOG_ERROR_MODBUS(msg)
LOG_WARNING_MODBUS(msg)
LOG_INFO_MODBUS(msg)
LOG_DEBUG_MODBUS(msg)
LOG_VERBOSE_MODBUS(msg)

// LED
LOG_ERROR_LED(msg)
LOG_WARNING_LED(msg)
LOG_INFO_LED(msg)
LOG_DEBUG_LED(msg)
LOG_VERBOSE_LED(msg)
```

### 2. Updated Files

#### `src/system.cpp`
- Added global log configuration variables
- Changed initialization message to use new logging system
- Updated verbose output during startup

**Changes:**
```cpp
// Before
PRINT(DEBUG_BASIC, F("."));

// After
LOG_VERBOSE_SYS(F("."));
LOG_INFO_SYS(F("[SYSTEM] Initialization complete\n"));
```

#### `src/eeprom_utils.cpp`
- Updated all EEPROM operations to use appropriate log levels
- Added category prefix `[EEPROM]` to all messages
- Changed info/debug messages to use correct log levels

**Changes:**
```cpp
// Before
PRINT(DEBUG_BASIC, F("EEPROM cleared\n"));
PRINT(DEBUG_BASIC, F("EEPROM saved\n"));
PRINT(DEBUG_BASIC, F("First time programming\n"));

// After
LOG_INFO_EEPROM(F("[EEPROM] EEPROM cleared successfully\n"));
LOG_INFO_EEPROM(F("[EEPROM] Configuration saved to EEPROM\n"));
LOG_INFO_EEPROM(F("[EEPROM] First boot detected - loading defaults\n"));
LOG_DEBUG_EEPROM(F("[EEPROM] Configuration loaded from EEPROM\n"));
LOG_DEBUG_EEPROM(F("[EEPROM] No changes detected, skipping save\n"));
```

#### `src/led.cpp`
- Updated LED initialization and status reporting
- Added structured logging for LED operations

**Changes:**
```cpp
// Before
PRINT(DEBUG_BASIC, "LED" + String(i+1) + ": State=...");

// After
LOG_INFO_LED(F("[LED] Initializing LED strips...\n"));
LOG_INFO_LED(F("[LED] LED initialization complete\n"));
LOG_INFO_LED(F("[LED] Status Report:\n"));
LOG_INFO_LED("  LED" + String(i+1) + ": State=...");
```

#### `src/main.cpp`
- Updated all Modbus-related messages to use `LOG_INFO_MODBUS`
- Updated LED control messages to use `LOG_DEBUG_LED`
- Changed max on-time warnings to use `LOG_WARNING_LED`
- Added category prefixes to all messages

**Changes:**
```cpp
// Before
PRINT(DEBUG_BASIC, F("Saving configuration to EEPROM via Modbus\n"));
PRINT(DEBUG_BASIC, F("Factory Reset (Except ID) via Modbus\n"));
PRINT(DEBUG_BASIC, "Max on-time exceeded for LED...");

// After
LOG_INFO_MODBUS(F("[MODBUS] Saving configuration to EEPROM\n"));
LOG_INFO_MODBUS(F("[MODBUS] Factory reset (except ID) requested\n"));
LOG_WARNING_LED("[LED] LED" + String(i+1) + " max on-time exceeded, turning off\n");
LOG_DEBUG_LED("[LED] LED" + String(i+1) + " turned ON\n");
LOG_DEBUG_LED("[LED] LED" + String(i+1) + " turned OFF\n");
```

### 3. Documentation

#### Created `doc/logging_system.md`
Comprehensive documentation covering:
- Log levels and their use cases
- Log categories and bit flags
- Configuration examples
- Usage examples for all macros
- Best practices
- Performance considerations

## Benefits

### 1. **Better Organization**
- Logs are now grouped by module/category
- Easy to enable/disable specific categories

### 2. **Improved Readability**
- Consistent prefixes: `[SYSTEM]`, `[EEPROM]`, `[MODBUS]`, `[LED]`
- Clear log levels indicate message importance

### 3. **Flexible Control**
- Control log verbosity globally with `globalLogLevel`
- Enable/disable specific categories with `enabledLogCategories`
- Can combine multiple categories using bitwise OR

### 4. **Production Ready**
- Easy to disable all logs: `globalLogLevel = LOG_NONE`
- Can show only errors: `globalLogLevel = LOG_ERROR`
- Category filtering reduces noise

### 5. **Developer Friendly**
- Clear API with self-documenting macro names
- Easy to add new categories

### 6. **Performance**
- No overhead when logs are disabled
- Compile-time optimizations possible
- Efficient bit-flag checking

## Configuration Examples

### Development Mode
```cpp
globalLogLevel = LOG_DEBUG;
enabledLogCategories = LOG_CAT_ALL;
```

### Production Mode
```cpp
globalLogLevel = LOG_WARNING;
enabledLogCategories = LOG_CAT_SYSTEM | LOG_CAT_MODBUS;
```

### Debugging Specific Module
```cpp
globalLogLevel = LOG_VERBOSE;
enabledLogCategories = LOG_CAT_LED;  // Only LED logs
```

### Silent Mode
```cpp
globalLogLevel = LOG_NONE;
enabledLogCategories = LOG_CAT_NONE;
```

## Testing

- ✅ All files compile without errors
- ✅ No breaking changes to existing functionality
- ✅ Documentation complete

## Future Enhancements

Possible future improvements:
1. Add timestamp to log messages
2. Add log output redirection (SD card, network, etc.)
3. Add runtime log level control via Modbus
4. Add circular buffer for log history
5. Add colored output support for terminal
6. Add file/line number information for debug builds

## References

- Main documentation: `doc/logging_system.md`
- Header file: `include/system.h`
- Implementation: `src/system.cpp`
