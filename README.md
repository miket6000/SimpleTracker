# SimpleTracker

SimpleTracker is a low cost and easy to use GPS tracker for model rocketry. It can operate of a single 3.7V lipo battery and has a built in charger to enable "plug-&-play" usability. Power on is can be done with a simple jumper, or an external switch can be used if perfered, for example via a screw switch. The GPS coordinates of the SimpleTracker are transmitted via 433MHz LoRa every second providing several km of tracking range depending on the antenna used on the SimpleTracker and the ground station. SimpleTracker can also be used as a minimal ground station, reporting the location of a paired tracker via USB serial connection to a PC or phone.

A sample python application is also provided which provides two receiver divesity to allow multiple antenna to be used to ensure there are no polarization blind spots, or to allow a directional antenna to be used with a omnidirectional backup.

## Hardware

The hardware is fairly minimal. It consist of a battery charger, two 3.3V linear power supplies, a LoRa radio module, GPS and a micro controller with a few LEDs to tie the parts together and provide status updates. Power is turned on by placing a header jumper between the two pins near the USB C connector. Bootmode can be enabled by shorting the 'b' and 'v' pins together on the PCB while placing the power on header. This will put the device into DFU mode so that it can be programmed using dfu-util or similar.

## Firmware

The firmware is currently a work in progress but the current status (at least when this was written) is operational. The GPS is read every 2 seconds to get a position update. This is then sent to the LoRa module to be transmitted. LoRa is currently configured by re-programming. Default settings are to broadcast on 434MHz. With the broadcast message consisting of a UID and the NMEA GPGGA message, this results in an on-air time of ~600ms every 2 seconds and a proven range of ~6km with a simple omnidirectional 1/4wave antenna.

The firmware has two modes that are currently selected and programmed by setting the "receive\_mode" variable to either true, or false. If set to true, the software acts as a ground station. It still determines it's location, but does not transmit it. Instead it sits there waiting to receive messages and forwards them on the UART. If "receive\_mode" is set to false, the firmware is configured as a transmitter and will broadcast it's GPS NMEA GPGGA message every 2 seconds. 

The blue and green LEDs provide status information. The blue LED is configured to flash every time the GPS read is performed. It will flash once if it managed to talk to the GPS module, and twice if the GPS module is reporting a location fix. The green LED will flash once on receiving a LoRa message, and twice when it transmits one. 

In receive\_mode the tracker outputs three different messages.
 1. A local NMEA message (identified by the characters '<-') followed by an 8 character UID of the local device, and the raw NMEA GPGGA message.
 2. A remote NMEA message (identified by the characters '->') followed by the 8 character UID of the remote transmitter and the raw NMEA GPGGA message.
 3. The RSSI of the last message received from the remote transmitter (identified by the characters 'RSSI:')

## Application (track.py)

The application is very basic. It monitors up to two hard coded serial addresses and updates an internal dictionary with the latest data it has received if the NMEA CRC comes through as valid. On every change the dictionary is written out to a CSV file. The raw strings received from the serial port, as well as two copies of the CSV file are saved to disk. One CSV file is always called 'live.csv'. This makes it easy to create a qgis 'Delimited Text Layer' which automatically updates once per second to display the GPS track data on a map layer. Every time the application is run, this file is destroyed and started from scratch, which is why the second copy is saved with a filename pointing to the current date and time (e.g. log\_20250108-1058.csv).

