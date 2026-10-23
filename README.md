## Hardware and Software Requirements

### Hardware

The firmware is designed for the **Arduino Mega 2560**, based on the ATmega2560 microcontroller. The board provides the digital and analog interfaces required to connect the sensors, data acquisition modules, and control components used in the measurement system.

The firmware is intended to operate with the hardware components and pin assignments defined in the source code. Before uploading the program, check that the wiring matches the pin configuration specified in the firmware.

### Software

The firmware is developed using the following tools and frameworks:

- **Development environment:** Visual Studio Code.
- **IDE extension:** PlatformIO IDE.
- **Build framework:** Arduino framework.
- **Target architecture:** AVR.
- **Target board:** Arduino Mega 2560.

PlatformIO is used to configure the target board, manage the build environment, compile the firmware, and upload it to the microcontroller.

### Project Configuration

The target board, platform, framework, and build settings are defined in the `platformio.ini` configuration file located at the root of the project.

This file is required to build the firmware using PlatformIO. Open the project folder in Visual Studio Code with the PlatformIO IDE extension installed, then use the PlatformIO interface to compile and upload the program to the Arduino Mega 2560.

Make sure that the required libraries are installed and that the selected serial port corresponds to the connected board.