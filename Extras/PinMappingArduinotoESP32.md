## Pin Mapping Guide: Arduino Uno to ESP32

This table maps Arduino Uno pins to ESP32 GPIO pins used in the railway crossing system project, with descriptions of each pin's function.

| Arduino Uno Pin | ESP32 GPIO | Description                     |
|-----------------|------------|---------------------------------|
| D2              | GPIO4      | Train Detection Sensor A        |
| D3              | GPIO5      | Train Detection Sensor B        |
| D4              | GPIO18     | Train Detection Sensor C        |
| D5              | GPIO19     | Train Detection Sensor D        |
| D6              | GPIO21     | Track LED A                     |
| D7              | GPIO22     | Track LED B                     |
| D8              | GPIO23     | Track LED C                     |
| D9              | GPIO13     | Gate Servo 1                    |
| D10             | GPIO12     | Gate Servo 2                    |
| D11             | GPIO2      | Track LED D                     |
| D12             | GPIO15     | Gate Buzzer                     |
| D13             | GPIO14     | Gate LED                        |
| A0              | GPIO25     | Independent Servo (Solar Panel Cleaning) |
| A1              | GPIO26     | Security Sensor                 |
| A2              | GPIO27     | Security LED 1                  |
| A3              | GPIO32     | Security LED 2                  |
| A4              | GPIO33     | Security Buzzer                 |

### Notes
- **Arduino Uno Pin**: Refers to the pin numbering on an Arduino Uno board, provided for users familiar with Uno pin layouts.
- **ESP32 GPIO**: Specifies the General Purpose Input/Output (GPIO) pin on the ESP32 microcontroller used in the project.
- **Description**: Indicates the component or function assigned to each pin, based on the project's code.
- The project code directly uses ESP32 GPIO numbers, so this mapping is primarily for reference to aid users transitioning from Arduino Uno-based projects.
