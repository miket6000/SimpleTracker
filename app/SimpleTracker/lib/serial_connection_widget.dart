import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_libserialport/flutter_libserialport.dart';
import 'package:intl/intl.dart';
import 'serial_parser.dart';

class SerialConnectionWidget extends StatefulWidget {
  final String title;
  final SerialMessage sharedMessage;
  final void Function() onMessageReceived;
  final ValueChanged<bool> onConnectionChange;

  const SerialConnectionWidget({
    super.key,
    required this.title,
    required this.sharedMessage,
    required this.onMessageReceived,
    required this.onConnectionChange,
  });

  @override
  SerialConnectionWidgetState createState() => SerialConnectionWidgetState();
}

class SerialConnectionWidgetState extends State<SerialConnectionWidget> {
  List<String> availablePorts = SerialPort.availablePorts;
  String? selectedPort;
  SerialPort? port;
  SerialPortReader? reader;
  List<String> receivedDataList = [];
  RandomAccessFile? logFile;
  bool isConnected = false;
  Timer? communicationTimer;
  bool isCommunicating = false;

  void connect() {
    if (selectedPort == null) return;
    port = SerialPort(selectedPort!);
    if (!port!.openRead()) {
      print("Failed to open port");
      return;
    }

    String logFileName =
        "${widget.title}_serial_${DateFormat('yyyyMMdd-HHmmss').format(DateTime.now())}.log";
    logFile = File(logFileName).openSync(mode: FileMode.write);

    reader = SerialPortReader(port!);
    reader!.stream.listen((data) {
      try {
        setState(() {
          String incomingData = String.fromCharCodes(data);
          receivedDataList.add(incomingData);
          List<String> lines = incomingData.split('\n');
          for (String line in lines) {
            if (line.trim().isNotEmpty) {
              // Update the shared message rather than creating a new instance.
              widget.sharedMessage.parse(line.trim(), widget.title);
              widget.onMessageReceived();
            }
            logFile?.writeStringSync("$line\n");
            logFile?.flushSync();
          }
          isCommunicating = true;
          widget.onConnectionChange(isCommunicating);
          communicationTimer?.cancel();
          communicationTimer = Timer(Duration(seconds: 6), () {
            setState(() {
              isCommunicating = false;
              widget.onConnectionChange(isCommunicating);
            });
          });
        });
      } catch (e, stackTrace) {
        print("error processing serial data: \$e");
        print(stackTrace);
      }
    }, onError: (error) {
      print("Serial stream error: \$error");
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
    setState(() {
      if (!isConnected) {
        connect();
        isConnected = true;
      } else {
        disconnect();
        isConnected = false;
      }
      widget.onConnectionChange(isConnected);
    });
  }

  @override
  void dispose() {
    communicationTimer?.cancel();
    disconnect();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(children: [
      Text(widget.title),
      Row(
        children: [
          DropdownButton<String>(
            hint: Text("Select a Port"),
            value: selectedPort,
            onChanged: (String? newValue) {
              setState(() {
                selectedPort = newValue;
              });
            },
            onTap: () {
              setState(() {
                availablePorts = SerialPort.availablePorts;
              });
            },
            items: availablePorts.map((String port) {
              return DropdownMenuItem<String>(
                value: port,
                child: Text(port),
              );
            }).toList(),
          ),
          SizedBox(width: 9),
          ElevatedButton(
            style: ButtonStyle(
              fixedSize: WidgetStateProperty.all(
                  Size.fromWidth(180)), // Width: 200, Height: 50
            ),
            onPressed: toggleConnection,
            child: Text(isConnected ? "Disconnect" : "Connect"),
          ),
        ],
      )
    ]);
  }
}
