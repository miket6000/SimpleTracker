import 'package:flutter/material.dart';
import 'settings.dart';

class ConfigurableSetting extends StatefulWidget {
  final Setting setting;
  const ConfigurableSetting({super.key, required this.setting});

  @override
  State<ConfigurableSetting> createState() => _ConfigurableSettingState();
}

class _ConfigurableSettingState extends State<ConfigurableSetting> {
  late TextEditingController _controller;
  late TextField valueTextField;
  int initialValue = 0;
  
  @override
  void initState() {
    initialValue = widget.setting.value;
    _controller = TextEditingController(text: initialValue.toString())
      ..addListener((){
        widget.setting.value = int.tryParse(_controller.text) ?? 0;
        setState((){});
      });
    valueTextField = TextField(controller:_controller);
    super.initState();
  }
  
  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left:10, right:10), 
          child: SizedBox(
            width:200,
            child: Text(widget.setting.title),
          ),
        ),
        widget.setting.optionList != null
        ? SizedBox ( width:150, child: DropdownButton<int>(
          isExpanded: true,
          value: widget.setting.value,
          onChanged: (int? newValue) {
            if (newValue != null) {
              setState(() {
                widget.setting.value = newValue;
              });
              //widget.onValueChanged(newValue);
            }
          },
          items: widget.setting.optionList!.entries.map((entry) {
            return DropdownMenuItem<int>(
              value: entry.value,
              child: Text(entry.key),
            );
          }).toList(),
        ) )
        : SizedBox(
          width:150,
          child: valueTextField,
        ),
        Padding(
          padding: const EdgeInsets.only(left:10),
          child: Tooltip(
            message: widget.setting.hint,
            child: Container(width:20, height:20, alignment:const Alignment(0, 1), decoration: const BoxDecoration(shape: BoxShape.circle , color: Colors.blue), child: const Text("i"),),
          )
        ) 
      ]
    );
  }
}

