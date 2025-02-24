import 'package:flutter/material.dart';

class DataTile extends StatelessWidget {
  final String title;
  final String? value;
  final VoidCallback? onPressed;

  const DataTile({
    Key? key,
    required this.title,
    this.value,
    this.onPressed,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // Use theme colors for the background and text.
    return InkWell(
      onTap: onPressed,
      child: Ink(
        padding: EdgeInsets.all(8.0),
        decoration: BoxDecoration(
          color: Theme.of(context).cardColor, // Uses card color from the theme
          borderRadius: BorderRadius.circular(8.0),
          boxShadow: [
            BoxShadow(
              color: Theme.of(context).shadowColor,
              blurRadius: 4.0,
              spreadRadius: 1.0,
            ),
          ],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(
              title,
              style: Theme.of(context).textTheme.headlineSmall, // Uses theme's headline6 style
            ),
            SizedBox(height: 8.0),
            Text(
              value ?? "N/A",
              style: Theme.of(context).textTheme.bodyMedium, // Uses theme's body text style\n",
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}