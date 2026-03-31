# SimpleTracker

![Photo comparing SimpleTracker to an Elvin Beacon for size](docs/SimpleTracker_V5_and_Elvin_Beacon.jpg "Comparison of SimpleTracker \(on the left\) and Elvin Beacon")

SimpleTracker is a low cost and easy to use GPS tracker for model rocketry. It can operate of a single 3.7V lipo battery and has a built in charger to enable "plug-&-play" usability. The GPS coordinates of the SimpleTracker are transmitted via 433MHz LoRa every second providing several km of tracking range depending on the antenna used on the SimpleTracker and the ground station. SimpleTracker can also be used as a minimal ground station, reporting the location of a paired tracker via USB serial connection to a PC or phone.

## Hardware

The hardware is fairly minimal. It consist of a battery charger, two 3.3V linear power supplies, a LoRa radio module, GPS and a micro controller with a couple of LEDs to provide status. Bootmode can be enabled by removing the battery and shorting the 'b' and 'v' pins together on the PCB while plugging the tracker into USB. This will put the device into DFU mode so that it can be programmed using dfu-util or STM's programming utilities.

The tracker can be powered by either USB (such as when used as a ground station) or via an AltusMetrum compatible 1s lithium battery. There is an included battery charger that will charge the battery at 100mA if USB is plugged in, so it is recommended that the tracker battery be between 100mAh (to avoid charging it too fast) and 300mAh (to avoid taking too long to charge). Of course there is no upper limit on the capacity of the battery, you can always unplug it to use an external charger if the charge is too slow.

## Firmware
The firmware is "good enough". It can be flashed to the tracker by disconnecting the battery and shorting the "boot" pin to the 3V3 pin right beside it and then plugging in the USB C cable. This will put the tracker into DFU bootloader mode where it can be programmed using CubeIDE or dfu-util.

On power up the module will listen on 434MHz SF 9 BW 125kHz for a command asking for it's UID, or telling it to start transmitting on another channel. Alternatively it can recieve a command via USB if it's acting as a ground station in which case it will issue commands to other trackers. This is all orchestrated by the application.

The LED provides status information via as follows:
 - 1 flash followed by 1 flash - Tracker mode, Waiting for GPS fix
 - 1 flash followed by 2 flashes - Tracker mode, GPS fix is valid
 - 2 flashes followed by 1 flash - Ground station mode, waiting for remote tracker GNSS fix
 - 2 flashes followed by 2 flashes - Ground station mode, remote tracker has GNSS fix
 - 3 flahses - Ground station mode, no remote tracker detected on the current channel in the last 10 seconds

## Application

I decided the application deserved it's own repository, so has been moved to [https://github.com/miket6000/SimpleSuite](https://github.com/miket6000/SimpleSuite). It is built and tested in Linux, with plans on supporting Android. 

