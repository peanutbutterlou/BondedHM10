# BondedHM10

BondedHM10 is an Arduino Library that facilitates the provisioning, connection and communication between two HM-10 Bluetooth LE 4.0 modules.

<br />

## Overview

The purpose of the BondedHM10 library is to make it extremely easy to enable wireless communication over two remote HM-10 BLE 4.0 modules that are intended to remain *permanently* conected to each other. It does this by automating the configuring (a.k.a. "provisioning") of the two HM-10 modules, by maintaining a persistent connection between them, and by handling the sending/receiving of messages and events. The BondedHM10 library also makes an effort to secure the connection by only allowing the Central (or "master") module to directly connect to a Peripheral (or "slave") module by MAC address, and by whitelisting the Central module's MAC address on the Periipheral, making it so that our Central module is the only one that can possibly connect to it. 

*NOTE: In addition, when it comes to securing the connection between HM-10 modules, the BondedHM10 library will disallow the Central module from announcing itself to nearby BLE devices. The MAC addresses will remain hardcoded in the Arduino sketches of the two remote devices and never be sent out over-the-air.*

In terms of the BLE 4.0 protocol, a "bonded" connection is an encrypted connection between two "paired" devices where a semi-perminant encryption key has been exchanged. This encryption key is stored locally on the devices and used every time the devices connect to each other, or until they are unpaired.  The BondedHM10 library will attempt to achieve a *true* bonded connection between the two devices, but is dependant on the HM-10 module to actually support the authentication/encryption required.

<br />

*NOTE: If the BondedHM10 library doesn't do exactly what you are expecting it to, I encourage you to fork this project and build on the foundation the library provides.*

<br />

## Requirements

- Two HM-10 modules are required; one to be the Central, and a second to be the Peripheral.
- In order for the two modules to communicate with each other, they will both first need to be provisioned as either a Central or Peripheral. Refer to the "BondedHM10_Central_Provision" and "BondedHM10_Peripheral_Provision" examples Arduino sketches for how to perform this one-time provisioning procedure.
- Both modules must be flashed with the 5.27 (or higher) version of the Jinan Huamao firmware.
- Two pins on the HM-10 (P11 and PIO2) will need to be connected to the Arduino board. P11 is an INPUT used to trigger a Reset of the HM-10 and PIO2 is an OUPUT used to detect whether or not the module is currently connected to its companion. Here is a image of the Central module in my own project with hookup wire soldered on to the two pins: ![Central HM10 Module with hookup wires soldered to the P11 and PIO2 pins](https://github.com/peanutbutterlou/BondedHM10/raw/master/images/BondedHM10_Soldered%20Pins.JPG)

<br />

## Features

- Performs all necessary provisioning of the HM-10 modules for both the Central and Peripheral roles.
- Establishes and maintains a persistent connection between the Central and Peripheral devices. Optionally allows the Central device to attempt to automatically reconnect to the Peripheral at a configurable time interval if the two devices become disconnected.
- Optionally allows the Central device to attempt to connect to the Peripheral as soon as the device is powered and ready.
- Allows the assignment of a callback/handler function to be invoked when a connection has been established.
- Allows the assignment of a callback/handler function to be invoked if and when the HM-10 disconnects from its counterpart.
- Ensures that both devices can only connect to each other.
- Handles the sending/receiving of custom messages and events between devices.
- Allows the assignment of a callback/handler function to be invoked whenever a custom message or event is received.
- Optionally handles the signaling of a configurable digital output pin that is written HIGH when the HM-10 module is connected to its remote counterpart. This feature can be used to turn on an LED whenever the devices are connected.
- Optionally handles the polling of a configurable digital input pin that triggers the local HM-10 to disconnect or reconnect to its counterpart. If the local HM-10 is connected to the remote and the input pin is read as LOW, it will disconnect; otherwise, if the local HM-10 is not connected, it will attempt to reconnect to its counterpart. This feature can be used to manually toggle on/off the wireless connection using a button or switch.
- Optionally handles the rapid signaling of a configurable digital output pin that is written HIGH for 50 miliseconds whenever the local HM-10 module is either sending or receiving data. This feature can be used to blink a LED when data is being transmitted.
- API for executing a subset of the AT commands available for the HM-10.
- Support for DEBUG and VERBOSE macro defines that will output extensive debug information about the HM-10's current configuration and its operation to the Serial Monitor.
- Optionally enable "Console Mode" that allows input from the Serial Monitor to be sent as raw UART data to the local HM-10 module. This feature can be used to manually invoke AT commands against the local HM-10 module for debugging and diagnostic purposes, or to manually send text to the remote device if it is connected.

<br />

## Limitations

- Supports only two HM-10 modules (one as the Central and the other as the Peripheral). No more. No less.
- Currently, BondedHM10 turns into a singleton object in your Arduino sketch when you use the built-in Data Transmission Output Pin feature and will only work when there is just once instance of the class. This is because the feature utilizes a Timer1 Interupt Service Routine (ISR) that needs to reference an internal static "active" instance of the BondedHM10 class. It's not terribly difficult to make the feature work with multiple BondedHM10 instances, it just wasn't a priority at the time of writing the library. If you find yourself needed this support, leave a message on the project and I'll get it done.
- Whether or not the communication between Central and Peripheral devices is encrypted is dependant on the HM-10 module you're using in your project supporting Authenticated and Bonded connections.

<br />

## Suggestions

- It's a good idea to use the same model and manufacturer for both modules to ensure compatibility. When developing this library I used two HM-10 modules manufactured by [DSD Tech](https://www.amazon.com/dp/B074VXZ1XZ).
- Check the operating voltage of your Arduino and HM-10 module. In my situation, I was using a SparkFun Redboard (which is essentially a modified Arduino Uno) which operates at 5V. The HM-10 modules I bought from DSD Tech (see Amazon link above) operate at 3.3V. If your HM-10 operates at a lower voltage than your Arduino board, it's a good idea to put a voltage divider in front of the HM-10's RX pin. I ended up using 3.6K and 1.8K ohm resisters in series to bring the Arduino's 5V signal down to 3.3V. Here is a image of the Central module in my own project with the voltage divider in front of the HM-10's RX pin: ![HM10 with Voltage Divider to the RX pin](https://github.com/peanutbutterlou/BondedHM10/raw/master/images/BondedHM10_Voltage%20Divider.JPG)
- For my own project, I used 22 AWG wire to hookup the P11 and PIO2 pins on the HM-10 module to the pins on the Arduino board. The output pins on the HM-10 are likely to be very very small leaving little room for error. When soldering the hookup wires onto the HM-10, I found it easiest to first transfer a small amount of solder from the tip of my iron directly onto the pads for the P11 and PIO2 pins. From there, I pressed the hookup wire on to the hardened solder and then applied heat directly to the wire, allowing it to sink into the solder already on the pads.

<br />

## Examples

- **BondedHM10_Central_Provision** - See how to provision an HM-10 module so that it can operate in "Central" mode and print out to the Serial Monitor the module's MAC address.
- **BondedHM10_Central** - See how to operate the HM-10 module in Central mode using the BondedHM10 library. This example will also demonstrate how to do the following:
    - Configure the Central to attempt to connect to the Peripheral on startup.
    - Configure the Central to attempt to auto-reconnect to the Peripheral, and how often to retry. This is useful in the event of a disconnect or if the connection attempt on startup failed.
    - How to handle 
- **BondedHM10_Peripheral_Provision** - See how to provision an HM-10 module so that it can operate in "Peripheral" mode and print out to the Serial Monitor the module's MAC address.
- **BondedHM10_Peripheral** - See how to operate the HM-10 module in Peripheral mode using the BondedHM10 library. This example will also demonstrate how to do the following:
    - sdsds
- **BondedHM10_ConsoleMode** - See how to put the BondedHM10 library into "Console" mode where, as either a Central or Peripheral, can execute AT commands directly against the local HM-10 module via the Serial Monitor input.

<br />

## Reference Material

- [HM-10 Bluetooth 4 BLE Modules](http://www.martyncurrey.com/hm-10-bluetooth-4ble-modules/)
- [BLE Pairing vs. Bonding](https://piratecomm.wordpress.com/2014/01/19/ble-pairing-vs-bonding/)