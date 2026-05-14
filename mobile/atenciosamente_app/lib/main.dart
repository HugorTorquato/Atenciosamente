import 'package:flutter/material.dart';
import 'screens/notifications_screen.dart';

// Flutter's entry point — same concept as main() in C++.
// runApp() inflates the widget tree and hands it to the Flutter engine.
void main() {
  runApp(const AtenciosamenteApp());
}

class AtenciosamenteApp extends StatelessWidget {
  const AtenciosamenteApp({super.key});

  @override
  Widget build(BuildContext context) {
    // MaterialApp is the root of a Material Design app. It sets up navigation,
    // theming, localization, and the overlay system. Every Flutter app has
    // exactly one of these at the top of the tree.
    return MaterialApp(
      title: 'Atenciosamente',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF1B1B2F),
        ),
        useMaterial3: true,
      ),
      home: const NotificationsScreen(),
    );
  }
}
