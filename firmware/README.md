# Rover
BLE controlled rover.

# References

**Arduino 101 (Intel ARC32)**

- [Datasheet](https://cdn.sparkfun.com/datasheets/Dev/Arduino/Boards/Arduino101Schematic.pdf)
- [Platform Info](https://docs.platformio.org/en/latest/platforms/intel_arc32.html#platform-intel-arc32)
- [Built-in Libaries](https://github.com/arduino/ArduinoCore-arc32/tree/master/libraries)

**Motor Shield Rev3 (L298P)**

Pinout

| Function        | Channel A | Channel B |
|-----------------|-----------|-----------|
| Direction       | D12       | D13       |
| Speed (PWM)     | D3        | D11       |
| Brake           | D9        | D8        |
| Current Sensing | A0        | A1        |

Note: 
- Operating voltage: 5-12V
- Max current: 2A per channel (4A max with external power supply)
- Current sensing: 1.65V/A

- [Schematics](https://docs.arduino.cc/resources/schematics/A000079-schematics.pdf)
- [Tutorial](https://docs.arduino.cc/tutorials/motor-shield-rev3/msr3-controlling-dc-motor/)
- [L298P Datasheet](https://www.st.com/content/ccc/resource/technical/document/datasheet/82/cc/3f/39/0a/29/4d/f0/CD00000240.pdf/files/CD00000240.pdf/jcr:content/translations/en.CD00000240.pdf)
