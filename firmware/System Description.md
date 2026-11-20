# System Description - Current State
The system being developed is a GPS tracker used for model rocketry. It consist of a number of identical modules which can act in different capacities depending on how the'y're configured.

- If the module is plugged into a PC it is considered a Ground Station. A Ground Station will receive commands from the PC and send them to other trackers and will receive packets from the other trackers and relay them to the PC.
- When powered on all trakers currently default to the Ground Station mode. This is to prevent trackers that are being charged or not ready to be used, but powered, on from cluttering the RF spectrum with transmissions.
- If instructed to send a command via USB, the Ground Station can transmit over LoRa to either a given UID, or a broadcast (UID ffffffff). The USB command to transmit is a 'T' followed by a space and the address operator '&' followed immediately by the 32 bit address and the command which is a single letter, e.g. "T &ffffffffU" would transmit to the broadcast address the command 'U' which ask devices to reply with their UID (using a random delay to minimize collisions).
- There are several "convenience" commands, such as PAIR which the ground station can received via USB e.g. PAIR ("PAIR [UID] [FREQ] [SF] [BW]" would tell the ground station to instruct the indicated remote tracker to change it's LoRa parameters to those indicated and also change the ground station's LoRa parameters to match as soon as the ACK is received)

## Workflow

1. PC instructs ground station to send UID LoRa broadcast on discovery channel (T &ffffffffU)
2. Remote trackers listen for broadcast and when received wait a random holdoff period before responding to the ground station with their UIDs.
3. PC request results of ground station discovery process (D)
4. User selects desired tracker from list presented.
5. PC sends PAIR command to ground station
6. Ground Station sends new channel parameters to tracker on discovery channel, but with specific remote tracker address.
7. Remote tracker responds with ACK then changes to new channel and starts transmitting GPS coordinates.
8. Ground Station receives ACK, and changes to new channel to receive GPS coordinates.

# Desired State

- There should be an ACK send by the Ground Station when it receives a packet from a remote tracker. That way the remote tracker could determine whether there was valid communication with the ground station. This would allow the remote tracker to drop to a low power mode after several minutes of no ACKs, where updates are sent less frequently, to conserve battery power.

# Constraints

- The processor is an STM32F042 so is very constrained on flash and RAM. At the moment 1K of Flash is dedicated to settings, however this might not be necessary with the evolving architecture. There are undoubtedly other optimizations that can be made to free up more space. 