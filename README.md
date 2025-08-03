# ESP32 Sensor Project

Embedded IoT project featuring an ESP32 microcontroller with MQTT communication. Designed for using it together with the **Telemetrix IoT-Dashboard** i created (check out [here](https://github.com/fepauly/telemetrix)).

## Features

- **WiFi Management**: Automatic connection with exponential backoff retry logic
- **MQTT Client**: Message publishing with queue-based offline buffering
- **FreeRTOS Integration**: Multi-task architecture with resource management
- **Event-Driven Design**: Asynchronous communication using event groups and queues

## Planned Features

- **Sensor Integration**: Integration of humidity sensor for indoor plant
- **TLS Security**: Encrypted MQTT communication with certificate validation (necessary for telemtrix)
- **Power Management**: Deep sleep modes for battery-powered operation
- **Better Configuration**: Better WiFi and MQTT credential setup

## Build & Deploy

### Prerequisites
- [PlatformIO](https://platformio.org/) Core
- ESP32 development board

### Build Configuration
Set WiFi credentials via build env flags in terminal:
```powershell
$env:WIFI_SSID="<ssid>"
$env:WIFI_PASS="<password>"
```

### Commands
```powershell
# Build project
pio.exe run

# Upload to ESP32
pio.exe run --target upload

# Monitor serial output
pio.exe device monitor
```

## Architecture

- **WifiManager**: Handles connection lifecycle with automatic reconnection
- **MqttManager**: Manages broker communication with message queuing
- **Task-based Design**: Separate tasks for WiFi, MQTT, and sensor publishing (currently dummy task)

## Developer Docs
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/)
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)