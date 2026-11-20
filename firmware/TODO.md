1. Ensure that transmissions are disabled while charging the battery.
- This could be done by detecting USB (currently unreliable) or by measuring the rail voltage. No, it can't. No battery voltage on the charger is 4.1V, nominal battery voltage is 4.2V, too close.
2. Need to determine if there is a usb connection... OR, turn on in ground station mode, and only transition to transmission on reception of a command from the ground station over LoRa... Try this.
3. Need to fix logic around ground station/tracker. I want to store the last paired tracker in a ground station so that I can easily reconnect to it and make it a setting for ease of use within the applicaiton. On power up if we get a "CONNECT REMOTE" command from the application we can just send the &xxxxxxT command to the remoteUid we have stored.. We can use the standard SET x 0123456 to save and recall the remoteUid, though there is some annoyance that this is decimal, not hex like the UID is naturally.

