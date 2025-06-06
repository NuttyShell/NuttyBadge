# _NuttyOS_

This is the NuttyOS, a custom designed OS based on ESP-IDF (which is based on FreeRTOS). For ESP-IDF Docs, please read its [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Basic Setup
You will need to install ESP-IDF before building this project. You can find guide to install ESP-IDF with Visual Studio Code [here](https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md).

After installing ESP-IDF, you can build the project within the VS Code, or just run `idf.py build`.

## Hardware Information
Hardware: ESP32-S3
Development Board: LuatOS ESP32S3 Development Board (Hardware schematic can be found [here](https://wiki.luatos.org/chips/esp32s3/hardware.html))

## How to build / flash / launch monitor
Hardware Connection: Just plug-in the USB-C Cable. Make sure the onboard DIP-switch is on the USB-Serial Function. (This is the default)

To Build: `idf.py build`

To Build and Flash: `idf.py flash`

To Run the Serial Monitor: `idf.py monitor`

Recommended: `idf.py flash && idf.py monitor` (This will auto build before flash)

## Folder contents

The project **NuttyOS** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main/
│   ├── drivers/               This is the directory containing drivers for hardware on the PCB
│   ├── services/              This is the directory containing services for NuttyOS
│   ├── CMakeLists.txt
│   ├── config.h               This is the config file to set some hard-coded values
│   ├── NuttyPeripherals.h     This is the file initializing ESP32 SoC Hardware peripherials
│   └── main.c
└── README.md                  This is the file you are currently reading
```



