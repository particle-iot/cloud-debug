# Cloud Debug - Tool for debugging cellular, Wi-Fi and cloud connectivity issues

## Installing

### Install the Particle CLI

If you haven't already installed the [Particle CLI](https://docs.particle.io/tutorials/developer-tools/cli/), so should do so it simplify installation.

### Update Device OS

The binaries target Device OS 1.5.2<sup>1</sup>. This means that Device OS 1.5.2 **or later** is required. If you're already installed 2.0.0 you can leave it on your device and still use these binaries.

If you have an older version of Device OS installed you will want to manually upgrade it by USB. While devices can update Device OS OTA (over-the-air), presumably if you need the cloud debugging tool you're having trouble connecting to the cloud, and thus will not be able to upgrade OTA.

- Connect the device by USB to your computer.
- Enter DFU mode (blinking yellow) by holding down MODE button and tapping RESET. Continue to hold dowh MODE while the status LED blinks yellow until it blinks magenta (red and blue at the same time) and release.
- Enter the following command in a Terminal or Command Prompt window:

```
particle update
```

<sup>1</sup>Except the Tracker SoM, which targets 1.5.4-rc.1.

### Download the binary

The binaries are included in Github as releases. The releases match the major and minor version of Device OS that's targeted. For example, 1.5.0 targets 1.5.x, in this case 1.5.2. As other versions of cloud-debug are released, those will have increasing patch versions, like 1.5.1, 1.5.2, etc.. 

Because binaries are generally compatible with later versions of Device OS, you don't need to have an exact match on the Device OS versions and you do not need to downgrade your Device OS binary.

When a version of cloud-debug is released for 2.0.x, it will start with version 2.0.0. However the 1.5.0 version of cloud-debug works just fine on 2.0.0-rc.1.

There is a pre-built binary for each device that is supported, such as:

| Gen   | Network | Name | Details |
| :---: | :------ | :--- | :--- |
| Gen 2 | Wi-Fi | photon.bin | |
| Gen 2 | Wi-Fi | p1.bin | |
| Gen 2 | Cellular | electron.bin | Also E Series, all SKUs |
| Gen 3 | Wi-Fi | argon.bin | |
| Gen 3 | Cellular | boron.bin | Both Boron 2G/3G and Boron LTE |
| Gen 3 | Cellular | bsom.bin | B402 LTE Cat M1 (North America) |
| Gen 3 | Cellular | b5som.bin | B523 LTE Cat 1 (Europe) |
| Gen 3 | Cellular | tracker.bin | Tracker SoM and Tracker One, both T402 and T523 |


### Flashing the binary to the device

## Viewing the logs

The easiest way to view the logs is to use the Particle CLI:

```
particle serial monitor
```

Note, however, that particle serial monitor is only a monitor. You cannot type commands into the window to control the device. For that, see the section below, Using the command line.

### Decoding the logs



## Using the cloud debug command line

The particle serial monitor command is handy for viewing the output from the cloud debug tool. However, you can't type commands to control the tool with it.

### Mac and Linux

On Mac and Linux, the `screen` command 

### Windows

PuTTY and CoolTerm


### Common commands

### Cellular commands

### Wi-Fi commands

## Cellular tower scan

Some devices with cellular modems can do a tower scan, which shows available GSM cellular carriers.

| Device | Tower Scan Supported |
| :--- | :---: |
| Electron G350 | &check; |
| Electron U260 | &check; |
| Electron U270 | &check; |
| E Series E310 | &check; | 
| E Series E402 | |
| Boron 2G/3G | &check; |
| Boron LTE | |
| B Series B402 | |
| B Series B523 | |
| Tracker T402 | |
| Tracker T523 | |

Of note:

- This does not work on LTE Cat M1 devices (E402, Boron LTE, B402) or Quectel devices (B523, Tracker SoM) as the hardware does not support it.
- It can only detect towers that support 2G/3G (not LTE or LTE Cat M1).
- It takes a few minutes to scan for towers.

To use the tower scan, use the `tower` command from the cloud debug command line, or tap the MODE button within the first 10 seconds after booting.


## Removing

You can remove the software by flashing another application, either your own, or a standard application like Tinker.

To do it using the CLI:

- Connect the device by USB to your computer.
- Enter DFU mode (blinking yellow) by holding down MODE button and tapping RESET. Continue to hold dowh MODE while the status LED blinks yellow until it blinks magenta (red and blue at the same time) and release.
- Enter the following command in a Terminal or Command Prompt window:

```
particle flash --usb tinker
```

## Building from source

### In Particle Workbench

- Clone [this repository](https://github.com/particle-iot/cloud-debug).
- In Particle Workbench use **Particle: Import Project** to load the project.
- Select your device type and Device OS version using **Particle: Configure Project For Device**
- Build (either local or cloud)!

Note that there are several libraries required. These are automatically loaded from the project.properties file.

- [CellularHelper](https://github.com/particle-iot/CellularHelper)
- [SerialCommandParserRK](https://github.com/rickkas7/SerialCommandParserRK)
- [CarrierLookupRK](https://github.com/rickkas7/CarrierLookupRK)

Because of the number of files and libraries, it's not particularly convenient to use the Web IDE; it's much easier to use Workbench or the CLI compiler.

### In Particle CLI

- Clone [this repository](https://github.com/particle-iot/cloud-debug).
- Build it using the Particle CLI (cloud compile):

```
cd cloud-debug
particle compile boron . --target 2.0.0-rc.2 --saveTo boron.bin
```


## Version History

### 0.0.1 (2020-09-24)

- Initial version



