//import 'package:intl/intl.dart';

class SerialMessage {
  String? rssi;
  String? uid;
  List<String>? nmeaFields;
  bool checksumValid = false;
  bool isGpsFix = false;
  double? latitude;
  double? longitude;
  double? altitude;
  DateTime? gpsTime;
  double? verticalVelocity;
  double? lastAltitude;
  DateTime? lastGpsTime;
  
  SerialMessage({this.rssi, this.uid, this.nmeaFields});

  String csvString() {
    return "$gpsTime, $uid, $latitude, $longitude, $altitude, $rssi\n";
  }

  double calculateVerticalVelocity(double? altitude, DateTime? time) {
    if (altitude == null || gpsTime == null || lastAltitude == null || lastGpsTime == null) {
      lastAltitude = altitude;
      lastGpsTime = gpsTime;
      return double.nan;
    }
    final timeDiff = gpsTime!.difference(lastGpsTime!).inSeconds;
    if (timeDiff == 0) return double.nan;
    final velocity = (altitude - lastAltitude!) / timeDiff * 3.28084; // Convert meters to feet
    lastAltitude = altitude;
    lastGpsTime = gpsTime;
    return velocity;
  }

  DateTime nmeaTime(String rawTime, {String? timeZone}) {
    int hours = int.parse(rawTime.substring(0, 2));
    int minutes = int.parse(rawTime.substring(2, 4));
    int seconds = int.parse(rawTime.substring(4, 6));

    DateTime utcTime = DateTime.utc(0, 1, 1, hours, minutes, seconds);
    //DateTime localTime = timeZone != null 
    //    ? utcTime.toLocal() 
    //    : utcTime;

    return utcTime.toLocal();
  }

  double nmeaToDec(String latLong, String dir) {
    double mul = (dir == "N" || dir == "E") ? 1.0 : -1.0;
    int divider = latLong.indexOf('.') - 2;
    int degrees = int.parse(latLong.substring(0, divider));
    double minutes = double.parse(latLong.substring(divider));
    return (degrees + minutes / 60) * mul;
  }

  void parse(String input) {
    input = input.trim();
    
    if (input.startsWith("RSSI")) {
      var parts = input.split(" ");
      if (parts.length > 1) {
        rssi = parts[1];
      }
    } else if (input.startsWith("->")) {
      var parts = input.split(" ");
      if (parts.length > 2) {
        uid = parts[1];
        nmeaFields = parts.sublist(2).join(" ").split(",");
        if(nmeaFields?.last == '*${calculateNmeaChecksum()?.toRadixString(16).toUpperCase()}') {
          checksumValid = true;
          isGpsFix = nmeaFields![6] == "1";
          if (isGpsFix) {
            latitude = nmeaToDec(nmeaFields![2], nmeaFields![3]);
            longitude = nmeaToDec(nmeaFields![4], nmeaFields![5]);
            altitude = double.parse(nmeaFields![9]) * 3.28084; // Convert meters to feet;
            verticalVelocity = calculateVerticalVelocity(altitude, gpsTime);
          }
          gpsTime = nmeaTime(nmeaFields![1]);
        } else {
          print('read: ${nmeaFields?.last}');
          print('calculated: *${calculateNmeaChecksum()?.toRadixString(16).toUpperCase()}');
          checksumValid = false;
        }
      }
    }
  }

  int? calculateNmeaChecksum() {
    if (nmeaFields == null || nmeaFields!.isEmpty) return null;
    String message = nmeaFields!.join(",");
    if (!message.contains("*")) return null;
    
    int checksum = 0;
    int endIndex = message.indexOf("*");
    for (int i = 1; i < endIndex; i++) {
      checksum ^= message.codeUnitAt(i);
    }
    return checksum;
  }

  @override
  String toString() {
    String retval = '';
    if (uid != null) retval += 'UID: $uid\n';
    if (rssi != null) retval += 'RSSI: $rssi\n';
    if (nmeaFields != null) {
      retval += 'NMEA: ${nmeaFields?.join(", ")}\n';
      retval += 'Checksum: ${checksumValid ? "OK" : "Failed"}\n';
    }
    return retval;
  }
}