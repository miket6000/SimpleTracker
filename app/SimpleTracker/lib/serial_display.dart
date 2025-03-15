import 'package:flutter/material.dart';

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