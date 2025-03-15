import 'package:flutter/material.dart';
import 'overview_screen.dart';
import 'settings_screen.dart';
void main() {
  runApp(SimpleTrackerApp());
}

class SimpleTrackerApp extends StatelessWidget {
  const SimpleTrackerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SimpleTracker',
      theme: ThemeData.dark(),
      debugShowCheckedModeBanner: false,
      home: DefaultTabController(
        length: 2, // Number of tabs
        child: MyHomePage(),
      ),
    );
  }
}


class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key});

  @override
  MyHomePageState createState() => MyHomePageState();
}

class MyHomePageState extends State<MyHomePage> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('SimpleTracker'),
        bottom: TabBar(
          tabs: [
            Tab(icon: Icon(Icons.home), text: "Overview"),
            Tab(icon: Icon(Icons.settings), text: "Settings"),
          ],
        ),
      ),
      body: TabBarView(
        children: [
          OverviewScreen(),
          SettingsScreen(),
        ],
      ),
    );
  }
}
