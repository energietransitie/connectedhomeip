# Does it Matter?

Graduation assignment on the application of the new industry standard 'Matter'
for the energy data measurement system of the research group Energy Transition
at Windesheim University of Applied Sciences

This branch holds changes that are made to the original repository in order to
conduct some [tests](#tests) for the research group Energy Transition at
Windesheim University of Applied Sciences.

## Table of contents

- [Does it Matter?](#does-it-matter)
  - [Table of contents](#table-of-contents)
  - [Tests](#tests)
  - [Running the all-clusters-app example on the ESP32](#running-the-all-clusters-app-example-on-the-esp32)
    - [Prerequisites](#prerequisites)
    - [Erasing all persistenly stored data](#erasing-all-persistenly-stored-data)
    - [Uploading firmware to the ESP32](#uploading-firmware-to-the-esp32)
  - [Running the all-clusters-app example on Linux](#running-the-all-clusters-app-example-on-linux)
    - [Prerequisites](#prerequisites-1)
    - [Running the Docker container](#running-the-docker-container)
  - [Issuing commands with the Linux chip-tool](#issuing-commands-with-the-linux-chip-tool)
    - [Prerequisites](#prerequisites-2)
    - [Issuing commands](#issuing-commands)
  - [Compiling the chip-tool for Linux](#compiling-the-chip-tool-for-linux)
    - [prerequisites](#prerequisites-3)
    - [Building the chip-tool](#building-the-chip-tool)
  - [Compiling the all-clusters-app example for the ESP32](#compiling-the-all-clusters-app-example-for-the-esp32)
    - [prerequisites](#prerequisites-4)
    - [Building the all-clusters-app](#building-the-all-clusters-app)
  - [Compiling the all-clusters-app example for Linux](#compiling-the-all-clusters-app-example-for-linux)
    - [prerequisites](#prerequisites-5)
    - [Building the all-clusters-app](#building-the-all-clusters-app-1)
  - [Credits](#credits)
  - [License](#license)

## Tests

The following functionalities were tested:

| Test                                           | Result             |
| ---------------------------------------------- | ------------------ |
| Building Matter for the ESP32                  | :heavy_check_mark: |
| Building Matter for Linux                      | :heavy_check_mark: |
| Commissioning commands                         | :heavy_check_mark: |
| Binding commands                               | :x:                |
| Binding to a node outside of the local network | :x:[^1]            |

[^1]: This test could not be conducted because the previous test failed.

## Running the all-clusters-app example on the ESP32

This section describes how you can deploy binary releases of the firmware, i.e.
without changing the source code, without a development environment and without
needing to compile the source code.

### Prerequisites

-   a device based on an ESP32 SoC, such as the
    [LilyGO TTGO T7 Mini32 V1.3 ESP32](https://github.com/LilyGO/ESP32-MINI-32-V1.3)
-   a USB to micro-USB cable
-   a PC with a USB port
-   [Python v3.8 or above](https://www.python.org/downloads/) installed, and
    make sure to select `Add Python <version number> to PATH` so you can use the
    Python commands we document below from a command prompt
-   [Esptool](https://github.com/espressif/esptool) installed, the Espressif SoC
    serial bootloader utility
-   [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/), a serial
    monitor utility (if your you are also developing, you may use the serial
    monitor utility in your IDE, instead).

### Erasing all persistenly stored data

Unless you are 100% sure that it is safe to only upload firmware and keep other
persistent memory intact, you should always first completely erase the
persistent (non-volatile) memory of the device.

-   Open a command prompt and enter:
    ```shell
    py -m esptool erase_flash
    ```
-   If the port is not detected automatically, enter (while replacing `?` with
    the digit found earlier): `shell py -m esptool erase_flash --port "COM?" `
    Should you encounter issues you may try to replace `py -m esptool` in the
    above commands with `python -m esptool` or `esptool.py`

### Uploading firmware to the ESP32

-   Connect the device with a USB cable to the PC.
-   Download the
    [binary release](https://github.com/energietransitie/connectedhomeip/releases)
    and extract it to a directory of your choice.
-   Some devices, such as the
    [LilyGO TTGO T7 Mini32 V1.3 ESP32](https://github.com/LilyGO/ESP32-MINI-32-V1.3),
    are based on the CH340 USB to serial converter, which may not be recognized
    by your OS. You may need to install a specific usb driver on your computer
    before you can upload firmware:
    -   [Windows driver](http://www.wch.cn/download/CH341SER_EXE.html);
    -   [Mac OSX driver](http://www.wch.cn/download/CH341SER_MAC_ZIP.html) (see
        also
        [this additional info](https://kig.re/2014/12/31/how-to-use-arduino-nano-mini-pro-with-CH340G-on-mac-osx-yosemite.html));
    -   [Linux driver](http://www.wch.cn/download/CH341SER_LINUX_ZIP.html);
    -   [Android driver](http://www.wch.cn/download/CH341SER_ANDROID_ZIP.html).
-   If you used the device before, you shoud first
    [erase all persistenly stored data](#erasing-all-persistenly-stored-data)
-   Open a comand prompt in the directory you chose for the download, change the
    directory to the subfolder `binaries` and enter:
    ```shell
    py -m esptool --chip esp32 --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 bootloader.bin 0x8000 partition-table.bin 0xe000 ota_data_initial.bin 0x10000 chip-all-clusters-app.bin
    ```
-   This should automatically detect the USB port that the device is connected
    to.
-   If not, then open the Device Manager (in Windows press the `Windows + X` key
    combination, then select Device Manager), go to View and click Show Hidden
    Devices. Then unfold `Ports (COM & LPT)`. You should find the device there,
    named `USB-Serial CH340 *(COM?)` with `?` being a single digit.
-   If the COM port is not automatically detected, then enter (while replacing
    `?` with the digit found in the previous step):
    ```shell
    py -m esptool --chip esp32 --port "COM?" --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 bootloader.bin 0x8000 partition-table.bin 0xe000 ota_data_initial.bin 0x10000 chip-all-clusters-app.bin
    ```
    Should you encounter issues you may try to replace `py -m esptool` in the
    above commands with
    ```shell
    python -m esptool
    ```
    or
    ```shell
    python3 -m esptool
    ```
    or
    ```shell
    esptool.py
    ```

## Running the all-clusters-app example on Linux

This section describes how you can deploy the all-clusters-app on Linux without
changing the source code, without a development environment and without needing
to compile the source code.

### Prerequisites

-   A Linux host
-   [Docker](https://docs.docker.com/engine/install/#server)
-   A user with docker
    [permissions](https://docs.docker.com/engine/install/linux-postinstall/)

### Running the Docker container

-   Create a new Docker volume:
    ```shell
    docker volume create matter_data
    ```
-   Create and run the Docker container:
    ```shell
    docker run -it \
    --name matter \
    --net=host \
    -v matter_data:/tmp \
    ghcr.io/energietransitie/matter-all-clusters-app:latest \
    --discriminator <discriminator> --passcode <passcode>
    ```
    Replace `<discriminator>` and `<passcode>` with the discriminator and
    passcode of your choice.

## Issuing commands with the Linux chip-tool

### Prerequisites

-   Linux host with the following dynamic libraries installed:
    -   libblkid1
    -   libc6
    -   libffi7
    -   libgcc-s1
    -   libglib2.0-0
    -   libglib2.0-dev
    -   libmount1
    -   libpcre2-8-0
    -   libpcre3
    -   libreadline8
    -   libselinux1
    -   libssl1.1
    -   libstdc++6
    -   libtinfo6
    -   zlib1g

### Issuing commands

-   Download the chip-tool binary from the
    [releases](https://github.com/energietransitie/connectedhomeip/releases) or
    [compile it yourself](#compiling-the-chip-tool-for-linux).
-   Change directory to where the binary executable is located.
-   Run command like this:
    ```shell
    ./chip-tool <commands>
    ```
    See the full list of commands [here](docs/guides/chip_tool_guide.md).

## Compiling the chip-tool for Linux

This section describes how you can change the source code using a development
environment and compile the source code into a binary release of the firmware
that can be deployed, using the development environment.

### prerequisites

-   Git
-   [Docker desktop](https://docs.docker.com/desktop/) for MacOS or Windows or
    [Docker](https://docs.docker.com/engine/) for Linux
-   [VS Code](https://code.visualstudio.com/)
-   [VS Code remote - containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

### Building the chip-tool

-   Clone the repository with git:
    ```shell
    git clone --branch does-it-matter https://github.com/energietransitie/connectedhomeip.git --recurse-submodules
    ```
    > This may take some time, because of the large amount of submodules.
-   Open the cloned repository in VS Code and click on `reopen in container` in
    the popup that will be displayed in the bottom right.
-   Open a terminal (CTRL+~) in VS code.
-   Run the following command:
    ```shell
    ./scripts/examples/gn_build_example.sh examples/chip-tool out/chip-tool
    ```
-   The binary executable can be found here: `out/chip-tool/chip-tool`.

## Compiling the all-clusters-app example for the ESP32

This section describes how you can change the source code using a development
environment and compile the source code into a binary release of the firmware
that can be deployed, using the development environment.

### prerequisites

-   Git
-   [Docker desktop](https://docs.docker.com/desktop/) for MacOS or Windows or
    [Docker](https://docs.docker.com/engine/) for Linux
-   [VS Code](https://code.visualstudio.com/)
-   [VS Code remote - containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

### Building the all-clusters-app

-   Clone the repository with git:
    ```shell
    git clone --branch does-it-matter https://github.com/energietransitie/connectedhomeip.git --recurse-submodules
    ```
    > This may take some time, because of the large amount of submodules.
-   Open the cloned repository in VS Code and click on `reopen in container` in
    the popup that will be displayed in the bottom right.
-   Open a terminal (CTRL+~) in VS code.
-   Change directory to the all-clusters-app folder:
    ```shell
    cd examples/all-clusters-app/esp32
    ```
-   Build the binaries:
    ```shell
    idf.py build
    ```
-   (Optionally) flash the binaries to the ESP32:
    ```shell
    idf.py build
    ```
-   (Optionally) open the monitor on the ESP32:
    ```shell
    idf.py monitor
    ```
-   (Tip) You can combine all the commands to build an flash the firmware and
    immediately :
    ```shell
    idf.py build flash monitor
    ```

## Compiling the all-clusters-app example for Linux

This section describes how you can change the source code using a development
environment and compile the source code into a binary release of the firmware
that can be deployed, using the development environment.

### prerequisites

-   Git
-   [Docker desktop](https://docs.docker.com/desktop/) for MacOS or Windows or
    [Docker](https://docs.docker.com/engine/) for Linux
-   [VS Code](https://code.visualstudio.com/)
-   [VS Code remote - containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)

### Building the all-clusters-app

-   Clone the repository with git:
    ```shell
    git clone --branch does-it-matter https://github.com/energietransitie/connectedhomeip.git --recurse-submodules
    ```
    > This may take some time, because of the large amount of submodules.
-   Open the cloned repository in VS Code and click on `reopen in container` in
    the popup that will be displayed in the bottom right.
-   Open a terminal (CTRL+~) in VS code.
-   Run the following command:
    ```shell
    ./scripts/build/build_examples.py --target linux-x64-all-clusters-no-ble-no-wifi build
    ```
-   The binary executable can be found here:
    `out/linux-x64-all-clusters-no-ble-no-wifi/chip-all-clusters-app`.

## Credits

This fork and the tests were created, conducted and described by:

-   Nick van Ravenzwaaij · [@n-vr](https://github.com/n-vr)

Product owner:

-   Henri ter Hofte · [@henriterhofte](https://github.com/henriterhofte) ·
    Twitter [@HeNRGi](https://twitter.com/HeNRGi)

## License

Matter is released under the [Apache 2.0 license](./LICENSE).
