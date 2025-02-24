import 'package:flutter/material.dart';
import 'status_indicator.dart';

class StatusBar extends StatelessWidget {
  final bool isGpsFix;
  final bool isConnected;

  const StatusBar({
    Key? key, 
    required this.isGpsFix, 
    required this.isConnected
  }) : super(key: key);


  @override
  Widget build(BuildContext context) {
    return Container(
      padding: EdgeInsets.all(8.0),
      color: Theme.of(context).scaffoldBackgroundColor,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          StatusIndicator(
            value: isGpsFix,
            activeText: "GPS Fix",
            inactiveText: "No GPS Fix",
          ),
          SizedBox(width:10),
          StatusIndicator(
            value: isConnected,
            activeText: "Connected",
            inactiveText: "No Connection",
          ),
        ],
      ),
    );
  }
}