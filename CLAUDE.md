# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP-IDF based IoT device firmware for ESP32-C2 that implements a UART-to-MQTT gateway. The device bridges serial communication from external hardware to cloud MQTT services, with WiFi provisioning via BLE and AP modes.

**Target Hardware**: ESP32-C2 (single core, 26MHz crystal)
**Flash Size**: 2MB
**Project Name**: gs_st

## Build System

This project uses ESP-IDF's CMake build system.

### Build Commands

```bash
# Configure the project (first time or after clean)
idf.py menuconfig

# Build the project
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor

# Flash and immediately monitor
idf.py flash monitor

# Clean build artifacts
idf.py fullclean
```

### SDK Configuration

- Main config: `sdkconfig` (generated, not typically edited directly)
- Default config: `sdkconfig.defaults` (version controlled)
- Partition table: `partitions.csv`

## Architecture

### Main Application Loop

Located in `main/main.c:app_main()`. Runs an event-driven loop with 10ms intervals that:
1. Processes events via `cc_event_run()`
2. Runs timers via `cc_timer_run()`
3. Handles HTTP operations via `cc_http_run()`
4. Executes timer tasks via `cc_tmr_task_run()`

### Core Subsystems (GS Layer)

The `main/gs/` directory contains the gateway services layer:

- **gs_main.c**: Initializes all GS subsystems (device, wifi, bind, mqtt, ota)
- **gs_device.c**: Device information management (versions, identifiers)
- **gs_wifi.c**: WiFi station and AP mode management, scan operations
- **gs_bind.c**: Device provisioning/binding logic, supports AP and BLE config modes
- **gs_mqtt.c**: MQTT client operations, message publish/subscribe with QoS support
- **gs_ota.c**: Over-the-air firmware update handling

### CC Component (Platform Abstraction)

Located in `components/cc/`, provides hardware abstraction:

- **cc/include/**: Core abstractions (events, timers, HTTP, logging, error codes)
- **port/include/**: ESP-IDF specific implementations (HAL layer for WiFi, KVS, system)

### Product Logic

`main/product.c` contains application-specific logic:

- **UART Communication**: UART1 @ 115200 baud (TX: GPIO20, RX: GPIO19)
- **Frame Protocol**: Custom binary protocol with header (0x23BB), command, length, data, CRC
- **UART→MQTT**: Receives frames from UART, converts to JSON, publishes to `/event/notify`
- **MQTT→UART**: Subscribes to `/service/publish`, converts JSON to frames, sends via UART
- **Button Handling**: GPIO9 with long press (2s) to reset bind status and reboot into config mode

### Frame Protocol

Binary frames with this structure:
- Header: `0x23BB` (2 bytes)
- Command: 1 byte (0x01 for device→cloud, 0x02 for cloud→device)
- Length: 1 byte (data length)
- Data: variable length (max 27 bytes given 32 byte total limit)
- CRC: 1 byte (custom CRC-8 algorithm)

### Supporting Components

- **rbuffer**: Ring buffer implementation for data buffering
- **http_client**: HTTP client library (AliOS Things origin)
- **flexible_button**: Button debouncing and event detection library
- **captive_portal**: Captive portal for AP mode WiFi configuration
- **frame_parser**: Frame parsing for the binary UART protocol

## Configuration & Provisioning

### WiFi Provisioning Modes

- **AP Mode**: Device creates WiFi access point for configuration
- **BLE Mode**: Device advertises BLE service for configuration
- Bind status stored in NVS (non-volatile storage)

### Long Press Reset

Hold button on GPIO9 for 2 seconds to:
1. Reset bind status
2. Set boot auto-start config mode (AP | BLE)
3. Reboot device

## Partition Layout

From `partitions.csv`:
- **otadata**: OTA data partition (0x9000, 0x2000)
- **ota_0**: Primary firmware slot (0x10000, 0xF0000 ~960KB)
- **ota_1**: Secondary firmware slot (0x100000, 0xF0000 ~960KB)
- **kvs**: Key-value storage for device config (6KB)
- **nvs**: General non-volatile storage (4KB)
- **ini**: SPIFFS partition (3KB)
- **verify**: Verification data (1KB)

## MQTT Topics

- Device publishes to: `/event/notify` (type: "0001")
- Device subscribes to: `/service/publish` (type: "0002")
- Property posts: `/event/property/post`

Messages use JSON format with fields: `ver`, `type`, `data` (hex string), `seq_no`

## Development Notes

### Memory Constraints

ESP32-C2 is resource-constrained:
- Single core operation (FreeRTOS unicore)
- Optimized for size (CONFIG_COMPILER_OPTIMIZATION_SIZE)
- Stack sizes carefully tuned in `sdkconfig.defaults`
- BLE-only mode (no BR/EDR)

### Logging

Uses custom `cc_log.h` macros:
- `CC_LOGI(TAG, ...)`: Info level
- `CC_LOGD(TAG, ...)`: Debug level
- `CC_LOGE(TAG, ...)`: Error level
- `CC_LOGI_HEXDUMP(TAG, data, len)`: Hex dump logging

### Event System

Custom event system via `cc_event.h`:
- Event bases: `GS_WIFI_EVENT`, `GS_MQTT_EVENT`, `GS_BIND_EVENT`
- Register handlers with `cc_event_register_handler()`
- Events processed in main loop via `cc_event_run()`

### Adding New Components

1. Create directory in `components/`
2. Add `CMakeLists.txt` with `idf_component_register()`
3. List in main component's `REQUIRES` if needed
4. Include headers and use in application code
