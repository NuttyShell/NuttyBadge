# _NuttyOS_

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is the simplest buildable example. The example is used by command `idf.py create-project`
that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)



## How to build / flash / launch monitor
`idf.py build`

`idf.py flash`

`idf.py monitor`

Recommended: `idf.py flash && idf.py monitor` (This will auto build before flash)

## Example folder contents

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



