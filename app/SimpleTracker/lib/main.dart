import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import 'datatile.dart';
import 'status_bar.dart';
import 'serial_parser.dart';
import 'serial_connection_widget.dart';
import 'dart:io';

void main() {
  runApp(SimpleTrackerApp());
}

class SimpleTrackerApp extends StatelessWidget {
  const SimpleTrackerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Serial Port Reader',
      theme: ThemeData.dark(),
      home: SimpleTrackerScreen(),
    );
  }
}

class SimpleTrackerScreen extends StatefulWidget {
  const SimpleTrackerScreen({super.key});

  @override
  SimpleTrackerScreenState createState() => SimpleTrackerScreenState();
}

class SimpleTrackerScreenState extends State<SimpleTrackerScreen> {
  SerialMessage message = SerialMessage();
  List<String> receivedDataList = [];
  int isCommunicating = 0;
  DateTime lastMessageTime = DateTime.now();
  double altOffset = 0.0;
  RandomAccessFile? liveFile;

  toggleAbsoluteAlt() {
    if (altOffset > 0.0) {
      altOffset = 0.0;
    } else {
      if (message.altitude != null) {
        altOffset = message.altitude!;
      }
    }
  }

  logMessage(String title) {
    if (message.gpsTime != null && message.gpsTime != lastMessageTime) {
      lastMessageTime = message.gpsTime!;
      liveFile?.writeStringSync(message.csvString());
      liveFile?.flushSync();
    }
    setState(() {
      receivedDataList.add("[$title] ${message.rawString}");
    });
  }

  @override
  void initState() {
    super.initState();
    liveFile = File("live.csv").openSync(mode: FileMode.write);
    liveFile?.writeStringSync("gpsTime, uid, latitude, longitude, altitude, rssi\n");
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("SimpleTracker")),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          children: [
            Row( children:[
            SerialConnectionWidget(
              title: "Port 1",
              sharedMessage: message,
              onMessageReceived: () {
                logMessage("1");
              },
              onConnectionChange: (connected) {
                setState(() {
                  if(connected) { isCommunicating |= 1; }
                  else { isCommunicating &= ~1; }
                });
              },
            ),
            SizedBox(width: 100),
            SerialConnectionWidget(
              title: "Port 2",
              sharedMessage: message,
              onMessageReceived: () {
                logMessage("2");
              },
              onConnectionChange: (connected) {
                setState(() {
                  if(connected) { isCommunicating |= 2; }
                  else { isCommunicating &= ~2; }
                });
              },
            ), 
            ]),
            SizedBox(height:20),
            SizedBox(
              height: 170,
              child: LayoutBuilder(
                builder: (context, constraints) {
                  double tileWidth = constraints.maxWidth / 3;
                  double tileHeight = 80;
                  return GridView.count(
                    crossAxisCount: 3,
                    childAspectRatio: tileWidth / tileHeight,
                    mainAxisSpacing: 10,
                    crossAxisSpacing: 10,
                    children: [
                      DataTile(title: "Time", value: message.gpsTime != null ? DateFormat("HH:mm:ss").format(message.gpsTime!) : "N/A"),
                      DataTile(title: "Latitude", value: message.latitude != null ? message.latitude!.toStringAsFixed(4) : "N/A"),
                      DataTile(title: "Longitude", value: message.longitude != null ? message.longitude!.toStringAsFixed(4) : "N/A"),
                      DataTile(title: "RSSI", value: message.rssi != null ? message.rssi!.values.join(", ") : "N/A"),
                      DataTile(title: "Altitude", value: message.altitude != null ? "${(message.altitude! - altOffset).toStringAsFixed(1)} ft" : "N/A", onPressed: toggleAbsoluteAlt),
                      DataTile(title: "Vertical Velocity", value: message.verticalVelocity != null ? "${message.verticalVelocity!.toStringAsFixed(2)} ft/s" : "N/A"),
                    ],
                  );
                },
              ),
            ),
            SizedBox(height: 20),

            SerialDisplay(receivedDataList: receivedDataList),

            StatusBar(isGpsFix: message.isGpsFix && isCommunicating > 0, isConnected: isCommunicating > 0),
          ],
        ),
      ),
    );
  }
}

class SerialDisplay extends StatelessWidget {
  const SerialDisplay({
    super.key,
    required this.receivedDataList,
  });

  final List<String> receivedDataList;

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: LayoutBuilder(
        builder: (context, constraints) {
          return Container(
            width: double.infinity,
            height: constraints.maxHeight,
            decoration: BoxDecoration(
              color: Colors.black, // Background color of the text box
              borderRadius: BorderRadius.circular(8.0),
            ),
            child: SingleChildScrollView(
              reverse: true,
              child: ConstrainedBox(
                constraints: BoxConstraints(
                  minHeight: constraints.maxHeight,
                ),
                child: Padding(
                  padding: const EdgeInsets.all(8.0),
                  child: Text(
                    receivedDataList.join('\n'),
                    style: TextStyle(fontSize: 16, color: Colors.white),
                    softWrap: true,
                  ),
                ),
              ),
            ),
          );
        },
      ),
    );
  }
}