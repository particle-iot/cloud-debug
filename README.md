# Cloud Debug - Tool for debugging cellular, Wi-Fi and cloud connectivity issues

## Installing

### Install the Particle CLI

If you haven't already installed the [Particle CLI](https://docs.particle.io/tutorials/developer-tools/cli/), so should do so now. 

Once installed, you will enter Particle CLI commands in a Command Prompt or Terminal window.

### Update Device OS

The binaries target Device OS 1.5.2, except for the Tracker SoM. This means that Device OS 1.5.2 **or later** is required. If you're already installed 2.0.0 you can leave it on your device and still use these binaries.

The Tracker SoM including the Tracker Evaluation Board and Tracker One should include Device OS 1.5.4-rc.1 and does not need to be upgraded. The binaries for Tracker target 1.5.4-rc.1 and will work on that version and later.

If you have an older version of Device OS installed you will want to manually upgrade it by USB. While devices can update Device OS OTA (over-the-air), presumably if you need the cloud debugging tool you're having trouble connecting to the cloud, and thus will not be able to upgrade OTA.

- Connect the device by USB to your computer.
- Enter DFU mode (blinking yellow) by holding down MODE button and tapping RESET. Continue to hold down MODE while the status LED blinks yellow until it blinks magenta (red and blue at the same time) and release.
- Enter the following command in a Terminal or Command Prompt window:

```
particle update
```

### Download the binary

The binaries are included in Github as releases. The releases match the major and minor version of Device OS that's targeted. For example, 1.5.1 targets 1.5.x, in this case 1.5.2. As other versions of cloud-debug are released, those will have increasing patch versions, like 1.5.1, 1.5.2, etc.. The last digit will continue to increase for each new version of the cloud-debug code.

Because binaries are generally compatible with later versions of Device OS, you don't need to have an exact match on the Device OS versions and you do not need to downgrade your Device OS binary.

When a version of cloud-debug is released for 2.0.x, it will start with a 2.0.x version with the next higher patch version. For example, it might be 2.0.3 if the last 1.5.x version was 1.5.3. Note that there could be both 1.5.3 and 2.0.3 if versions for both versions of Device OS are built. 

Since user firmware binaries are backward compatible, the 1.5.1 version will work on 2.0.x and 3.0.x, but if we need to take advantage of features only included in a later version we may include multiple binary targets in the future.

There is a pre-built binary for each device that is supported, such as:

| Gen   | Network  | Name | Details |
| :---: | :------- | :--- | :--- |
| Gen 2 | Wi-Fi    | photon.bin | |
| Gen 2 | Wi-Fi    | p1.bin | |
| Gen 2 | Cellular | electron.bin | Also E Series, all SKUs |
| Gen 3 | Wi-Fi    | argon.bin | |
| Gen 3 | Cellular | boron.bin | Both Boron 2G/3G and Boron LTE |
| Gen 3 | Cellular | bsom.bin | B402 LTE Cat M1 (North America) |
| Gen 3 | Cellular | b5som.bin | B523 LTE Cat 1 (Europe) |
| Gen 3 | Cellular | tracker.bin | Tracker SoM and Tracker One, both T402 and T523 |


### Flashing the binary to the device

- Connect the device by USB to your computer.
- Enter DFU mode (blinking yellow):

```
particle usb dfu
```

- Enter the following command in a Terminal or Command Prompt window:

```
cd Downloads
particle flash --usb boron.bin
```

Modify the cd command if you've saved the binary somewhere else. And of course replace boron.bin with the bin file for your device.


## Viewing the logs

The easiest way to view the logs is to use the Particle CLI:

```
particle serial monitor
```

Note, however, that particle serial monitor is only a monitor. You cannot type commands into the window to control the device. For that, see the section below, Using the command line.

### Decoding the logs

These application notes may help decode the logs.

- [AN003 Interpreting Cloud Debug](https://github.com/particle-iot/app-notes/tree/master/AN003-Interpreting-Cloud-Debug) shows how to interpret cloud debugging logs to troubleshoot various common issues.
- [AN004 Interpreting Cloud Debug-2](https://github.com/particle-iot/app-notes/tree/master/AN004-Interpreting-Cloud-Debug-2) is a deep dive into interpreting cloud debug logs and cross-referencing the AT command guide for the u-blox modem.

## Using the cloud debug command line

The particle serial monitor command is handy for viewing the output from the cloud debug tool. However, you can't type commands to control the tool with it.

### Mac and Linux

On Mac OS (OS X) and Linux systems, you can access the serial port through the terminal.

For Mac OS, open the terminal and type:

```
screen /dev/tty.u
```

and pressing tab to autocomplete.

On Linux, you can accomplish the same thing by using:

```
screen /dev/ttyACM
```

and pressing tab to autocomplete.

Now you are ready to read data sent by the device over Serial and send data back.


### Windows

For Windows users, we recommend downloading [PuTTY](http://www.putty.org/). Plug your device into your computer over USB, open a serial port in PuTTY using the standard settings, which should be:

- Baud rate: 9600
- Data Bits: 8
- Parity: none
- Stop Bits: 1


### Cellular commands

#### cellular

```
> cellular -c
> cellular -d
```

Connect (-c) or disconnect (-d) from cellular. This only will work reliably after you've successfully connected to the cellular. Executing this during a cloud test may fail.

#### setCredentials

```
> setCredentials -a "broadband"
```

Sets the APN, and optionally the username and password, for a 3rd-party SIM card. 

| Parameter | Details | Required |
| :--- | :--- | :--- |
| -a | APN | Yes |
| -u | Username | No |
| -p | Password | No |


Boron: This setting is persistent and will stay in effect through reboots and Device OS upgrades.

Electron 2G/3G: Supported, however the setting is not persistent and will be reset when the cellular modem is powered down.

B Series SoM, Tracker SoM, E Series, Electron LTE: Not supported as these devices do not support 3rd-party SIM cards.

#### clearCredentials

```
> clearCredentials
```

Clears the APN, username, and password for a 3rd-party SIM card. This setting is persistent and will stay in effect through reboots and Device OS upgrades.

Supported on the Boron only. The B Series SoM and Tracker SoM do not support 3rd-party SIM cards.

On the Electron and E Series, removing all power (battery and USB) for 10 seconds will reset credentials.

#### setActiveSim

```
> setActiveSim -i
> setActiveSim -e
```

Sets the active SIM card to internal Particle SIM (-i) or external (-e). This setting is persistent and will stay in effect through reboots and Device OS upgrades.

Supported on the Boron only. The B Series SoM and Tracker SoM do not support 3rd-party SIM cards.

Note: On the Boron LTE you must not only set the SIM to internal (-i) but you must physically remove the SIM card from the 4FF nano SIM card slot as well.


#### mnoprof

```
> mnoprof 100
```

Sets the Mobile Network Operator Profile number on Boron LTE Cat M1 devices. Setting this incorrectly will prevent the device from connecting to cellular (stuck on blinking green). 

You will need to reset the modem after setting the MNO profile. Sending the command `AT+CFUN=15` is one way.

Supported on the Boron LTE only. The B Series SoM and Tracker SoM do not support 3rd-party SIM cards. This setting is persistent and will stay in effect through reboots and Device OS upgrades.

#### command

```
> command AT
> command AT+COPS=3
```

Send a raw AT command to the modem. Everything after "command" is passed directly to Cellular.command, and a `\r\n` is appended to the end. Trace logging is enabled for the duration of the command so you can see the results.

### Wi-Fi commands

#### wifi

```
> wifi -c
> wifi -d
```

Connect (-c) or disconnect (-d) from Wi-Fi. This only will work reliably after you've successfully connected to the Wi-Fi. Executing this during a cloud test may fail.

#### antenna 

```
> antenna -i
> antenna -e
> antenna -a
```

Select the Wi-Fi antenna on the Photon, P0, and P1. This option is persistent and will stay in effect through reboots, Device OS upgrades, and reset credentials.

| Option | Use | Details |
| :--- | :--- | :--- |
| -i | Internal | Default on Photon and P1 |
| -e | External | External (U.FL connector) |
| -a | Automatic | Available on Photon and P1, not normally enabled |

The Argon does not have an internal antenna and does not support this option.

#### clearCredentials

```
> clearCredentials
```

This command clears Wi-Fi credentials. 

Supported on the Photon, P1, and Argon.


#### setCredentials

```
> setCredentials -s "My Network" -p "secret"
> setCredentials -s "My Network" -p "secret" -t WPA
```

This command sets Wi-Fi credentials. The setting is persistent and survives reset and Device OS upgrades.

| Parameter | Details | Required |
| :--- | :--- | :--- |
| -s | SSID | Yes |
| -p | Password | Yes, unless the network is open/unsecured |
| -t | Authentication Type | No, unless the AP is not available |

The authentication type is inferred when the network is able to be connected to when setCredentials is called. However, if you are pre-configuring authentication credentials for networks not available at the time of the call, you must specify the authentication type (case-sensitive):

- WEP
- WPA
- WPA2

Supported on the Photon, P1, and Argon.

#### useDynamicIP

```
> useDynamicIP
```

Sets dynamic IP address mode. This is the factory default, and resets a static IP set with useStaticIP. 

This option is persistent and will stay in effect through reboots, Device OS upgrades, and reset credentials.

Supported on the Photon and P1 only. The Argon does not support static IP addresses.

#### setStaticIP

```
> setStaticIP -a 192.168.1.5 -s 255.255.255.0 -g 192.168.1.1 -d 192.168.1.1
```

Sets static IP address mode. This option is persistent and will stay in effect through reboots, Device OS upgrades, and reset credentials. All four parameters are required:

| Parameter | Details | Required |
| :--- | :--- | :--- |
| -a | IP address | Yes |
| -s | Subnet mask | Yes |
| -g | Gateway address | Yes |
| -d | DNS server address | Yes |

Supported on the Photon and P1 only. The Argon does not support static IP addresses.

### Common commands

#### keepAlive

```
> keepAlive 5
```

Set the Particle Cloud keep-alive value in minutes. 

This is available on:

- Gen 2 cellular devices (Electron, E Series)
- Gen 3 devices (Argon, Boron, B Series SoM, Tracker SoM)

#### cloud

```
> cloud -c
> cloud -d
```

Connect (-c) or disconnect (-d) from the cloud. This only will work reliably after you've successfully connected to the cloud. Executing this during a cloud test may fail.

#### trace

```
> trace -e
> trace -d
```

Enable (-e) or disable (-d) trace logging. This affects how verbose the logs are. On the Electron/E Series in particular, the logs get very long in trace mode.

#### safeMode

```
> safeMode
```

Enter safe mode (breathing magenta). However, if you are having cloud connection problems, this may not work as in order to enter safe mode, you need to be able to connect to the cloud.

#### dfu

```
> dfu
```

Enter DFU mode (blinking yellow). It may make more sense to use `particle usb dfu` but this command is available.



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

You can also build cloud-debug from source. You may want to do this to customize behavior, such as logging to Serial1 (USART serial) instead of USB serial on a P1 or B Series SoM without USB. 

Note that you cannot target versions of Device OS older than 1.2.1. The code requires features built into that version and will not compile on earlier versions. 

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

### 1.5.1 (2020-09-28)

- Initial version



