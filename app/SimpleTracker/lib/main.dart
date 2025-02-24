import 'package:flutter/material.dart';
import 'package:flutter_libserialport/flutter_libserialport.dart';
import 'package:intl/intl.dart';
import 'datatile.dart';
import 'status_bar.dart';
import 'serial_parser.dart';
import 'dart:io';
import 'dart:async';

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
  List<String> availablePorts = SerialPort.availablePorts;
  String? selectedPort;
  SerialPort? port;
  SerialPortReader? reader;
  List<String> receivedDataList = [];
  SerialMessage message = SerialMessage();
  RandomAccessFile? logFile;
  RandomAccessFile? liveFile;
  bool isConnected = false;
  Timer? communicationTimer;
  bool isCommunicating = false;
  double altOffset = 0.0;

  toggleAbsoluteAlt() {
    if (altOffset > 0.0) {
      altOffset = 0.0;
    } else {
      if (message.altitude != null) {
        altOffset = message.altitude!;
      }
    }
  }

  void connect() {
    port = SerialPort(selectedPort!);

    if (!port!.openRead()) {
      print("Failed to open port");
      return;
    }

    String logFileName = "serial_${DateFormat('yyyyMMdd-HHmmss').format(DateTime.now())}.log";
    logFile = File(logFileName).openSync(mode: FileMode.write);
    liveFile = File("live.csv").openSync(mode: FileMode.write);

    liveFile?.writeStringSync("gpsTime, uid, latitude, longitude, altitude, rssi\n");

    reader = SerialPortReader(port!);
    reader!.stream.listen((data) {
      setState(() {
        String incomingData = String.fromCharCodes(data);
        List<String> lines = incomingData.split('\n'); 
        for (String line in lines) {
          if (line.trim().isNotEmpty) {
            receivedDataList.add(line.trim());
            message.parse(line.trim());
            logFile?.writeStringSync('$line\n');
            logFile?.flushSync();
            liveFile?.writeStringSync(message.csvString());
            liveFile?.flushSync();
          }
        }
        
        isCommunicating = true;
        communicationTimer?.cancel();
        communicationTimer = Timer(Duration(seconds: 6), () {
          setState(() {
            isCommunicating = false; // No data received for 6 seconds
          });
        });         
      });
    });
  }

  void disconnect() {
    reader?.close();
    port?.close();
    logFile?.closeSync();
    reader = null;
    port = null;
    logFile = null;
  }

  void toggleConnection() {
    if (selectedPort == null) return;

    if (!isConnected) {
      connect();
    } else {
      disconnect();
    }

    setState((){isConnected = !isConnected;});
  }

  @override
  void dispose() {
    reader?.close();
    port?.close();
    logFile?.closeSync();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("SimpleTracker")),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          children: [
            PortSelector(
              selectedPort: selectedPort,
              availablePorts: availablePorts,
              onPortChanged: (newPort) {
                setState(() {
                  selectedPort = newPort;
                });
              },
              onDropdownTap: () {
                setState(() {
                  availablePorts = SerialPort.availablePorts;
                });
              },
              toggleConnection: toggleConnection,
              isConnected: isConnected,
            ),
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
                      DataTile(title: "RSSI", value: message.rssi != null ? message.rssi.toString() : "N/A"),
                      DataTile(title: "Altitude", value: message.altitude != null ? "${(message.altitude! - altOffset).toStringAsFixed(1)} ft" : "N/A", onPressed: toggleAbsoluteAlt),
                      DataTile(title: "Vertical Velocity", value: message.verticalVelocity != null ? "${message.verticalVelocity!.toStringAsFixed(2)} ft/s" : "N/A"),
                    ],
                  );
                },
              ),
            ),
            SizedBox(height: 20),

            SerialDisplay(receivedDataList: receivedDataList),

            StatusBar(isGpsFix: message.isGpsFix && isCommunicating, isConnected: isCommunicating),
          ],
        ),
      ),
    );
  }
}


class PortSelector extends StatelessWidget {
  final String? selectedPort;
  final List<String> availablePorts;
  final ValueChanged<String?> onPortChanged;
  final VoidCallback onDropdownTap;
  final VoidCallback toggleConnection;
  final bool isConnected;

  const PortSelector({
    super.key,
    required this.selectedPort,
    required this.availablePorts,
    required this.onPortChanged,
    required this.onDropdownTap,
    required this.toggleConnection,
    required this.isConnected,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        DropdownButton<String>(
          hint: Text("Select a Port"),
          value: selectedPort,
          onChanged: onPortChanged,
          onTap: onDropdownTap,
          items: availablePorts.map((String port) {
            return DropdownMenuItem<String>(
              value: port,
              child: Text(port),
            );
          }).toList(),
        ),
        SizedBox(width: 9),
        ElevatedButton(
          onPressed: toggleConnection,
          child: Text(isConnected ? "Disconnect" : "Connect"),
        ),
      ],
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