# SimpleTracker

![Photo comparing SimpleTracker to an Elvin Beacon for size](docs/SimpleTracker_V5_and_Elvin_Beacon.jpg "Comparison of SimpleTracker \(on the left\) and Elvin Beacon")

SimpleTracker is a low cost and easy to use GPS tracker for model rocketry. It can operate of a single 3.7V lipo battery and has a built in charger to enable "plug-&-play" usability. The GPS coordinates of the SimpleTracker are transmitted via 433MHz LoRa every second providing several km of tracking range depending on the antenna used on the SimpleTracker and the ground station. SimpleTracker can also be used as a minimal ground station, reporting the location of a paired tracker via USB serial connection to a PC or phone.

## Hardware

The hardware is fairly minimal. It consist of a battery charger, two 3.3V linear power supplies, a LoRa radio module, GPS and a micro controller with a couple of LEDs to provide status. Bootmode can be enabled by removing the battery and shorting the 'b' and 'v' pins together on the PCB while plugging the tracker into USB. This will put the device into DFU mode so that it can be programmed using dfu-util or STM's programming utilities.

The tracker can be powered by either USB (such as when used as a ground station) or via an AltusMetrum compatible 1s lithium battery. There is an included battery charger that will charge the battery at 100mA if USB is plugged in, so it is recommended that the tracker battery be between 100mAh (to avoid charging it too fast) and 300mAh (to avoid taking too long to charge). Of course there is no upper limit on the capacity of the battery, you can always unplug it to use an external charger if the charge is too slow.

## Firmware

The firmware is currently a work in progress but the current status as of updating this document the following is working:
 - Flash driver to allow storing and updating of parameters.
 - LED driver
 - GNSS module driver
 - Command interpreter for configuration, Ground Station mode and debug

The below is still WIP:
 - The LoRa module Tx/Rx code

Default settings are to broadcast on 434MHz. With the broadcast message consisting of a UID and the NMEA GPGGA message, this results in an on-air time of ~600ms every 2 seconds and a proven range of at least 10km with a simple omnidirectional 1/4 wave antenna.

The LED provides status information via as follows:
 - 1 flash followed by 1 flash - Tracker mode, Waiting for GPS fix
 - 1 flash followed by 2 flashes - Tracker mode, GPS fix is valid
 - 2 flashes followed by 1 flash - Ground station mode, waiting for remote tracker GNSS fix
 - 2 flashes followed by 2 flashes - Ground station mode, remote tracker has GNSS fix
 - 3 flahses - Ground station mode, no remote tracker detected on the current channel in the last 10 seconds

In receive\_mode the tracker outputs three different messages.
 1. A local NMEA message (identified by the characters '<-') followed by an 8 character UID of the local device, and the raw NMEA GPGGA message.
 2. A remote NMEA message (identified by the characters '->') followed by the 8 character UID of the remote transmitter and the raw NMEA GPGGA message.
 3. The RSSI of the last message received from the remote transmitter (identified by the characters 'RSSI:')

## Application

I decided the application deserved it's own repository, so has been moved to [https://github.com/miket6000/SimpleSuite](SimpleSuite). It is built and tested in Linux, with plans on supporting Android. 

