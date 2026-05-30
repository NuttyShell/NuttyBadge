# NuttyBadge/NuttyHardware

---
# Hardware Changelog

## Latest hardware revision: Rev. B (June-2025)

Compared to Rev. A (Initial Version of June-2024), NuttyBadge Revision B has the following changes:

- LuatOS ESP32 Module has stopped production entirely. As the manufacturer refuses to sell the module with even no MOQ figures, we are forced to switch to use another module. We have completely abandoned the usage of 3rd party module and stick with original `ESP32-S3-WROOM-1-N16R8` module to avoid future supply chain problem.

- We included a USB HUB chip (CH334R) [U10] to allow the simultaneously usage of both usb-to-serial chip (CH343P) [U11] and ESP32S3 onboard USB device.

- Auto reset and program mode via DTR/RTS pins are fully implemented.

- Note: C49 and C51 values are incorrect, as CH334R contains an internal bypass cap for the crystal. The distributed boards had C49 and C51 removed. This will be fixed in NuttyBadge Rev. C.

- In terms of schematic hierarchy, we now have a dedicated schematic sheet for the ESP32S3 part of the circuit due to above reasons.

- Audio circuit is optmized and simplified. Now we just use a single N-FET (AO3400A) [Q4] to drive the buzzer [BZ1]. We expect this will further improve the sound quality and efficiency of the circuit.

- A Camera port is included in Rev. B. Although one already provided in the LuatOS module, the pins used might clash with PSRAM and are not carefully considered. Tested with OV2640 with fair results.

- Two LDOs are included to generate 1.2V and 2.8V rails to enable the usage of camera.

- LCD Backlight are driven with N-FET, this will make the driving circuit more efficient.

- For the main battery buck-boost converter, we have switched L1 from 2.2uH to 3.3uH, to provide better voltage ripple figures and reduce conduction losses in the expense of load transient response.

- We also wired the PS pin of TPS63001 to the positive rail, enabling power save mode to allow the overall circuit saving more power.
