import 'settings.dart';
import 'package:flutter/material.dart';
import 'configuable_setting.dart';

final GlobalKey<SettingPageState> settingPageKey = GlobalKey<SettingPageState>(); 

class SettingPage extends StatefulWidget {
  const SettingPage({super.key});
  @override
  State<SettingPage> createState() => SettingPageState();
}

class SettingPageState extends State<SettingPage> {
  List<ConfigurableSetting> settingWidgets = [];
  Map<String, int> initValues = {};

  void updateSettings() {
    setState(() {});
  }

  void erase() {
    setState(() {});
  }

  void factoryReset() {
    setState(() {});
  }

  void update() {
    setState(() {});
  }

  @override
  void initState() {
    for (var setting in settings.keys.where((e)=>settings[e]!.configurable)) {
      initValues[setting] = settings[setting]!.value;
      settingWidgets.add(ConfigurableSetting(setting: settings[setting]!));
    }
    super.initState();    
  }

  @override
  Widget build(BuildContext context) {
    const double widgetWidth = 350;
    bool wideScreen = MediaQuery.sizeOf(context).width > (widgetWidth * 2);

    return 
      SingleChildScrollView(child:
      Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
        children: [
        Flex(
          direction: wideScreen ? Axis.horizontal : Axis.vertical,
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceAround,
          children: [
             Container(
                constraints: const BoxConstraints(maxWidth: widgetWidth, maxHeight: 400),
                child: Column( 
                  children:[
                    const Text("SimpleTracker Settings"),
                    ...settingWidgets,
                  ]
                ),
              ),
          ]   
        ),
        Flex(
          direction: wideScreen ? Axis.horizontal : Axis.vertical,
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          crossAxisAlignment: CrossAxisAlignment.end,
          children:[
            Row(
              //mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              //crossAxisAlignment: CrossAxisAlignment.end,
              children:[
                ElevatedButton(
                  onPressed: factoryReset, 
                  child: const Text("Factory Reset"),
                ),
                const SizedBox(width:60),
                ElevatedButton(
                  onPressed: updateSettings, 
                  child: const Text("Upload Settings"),
                ),
              ]
            ),
          ],
        ),
        ]
      )
      )
    );
  }

}