import 'package:flutter/material.dart';
import '../api/notifications_client.dart';
import '../models/notification.dart';

// StatelessWidget is a widget with no mutable state — it renders purely from
// its constructor arguments. Think of it as a pure function: same input,
// same output. Use it when the widget doesn't need to react to changes over
// time. We use it here because the screen itself holds no state — the Future
// and its lifecycle are managed by FutureBuilder internally.
class NotificationsScreen extends StatelessWidget {
  const NotificationsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      // Scaffold is the standard Material screen skeleton: app bar, body,
      // floating action button, drawer, etc. Every screen gets one.
      appBar: AppBar(
        title: const Text('Atenciosamente'),
        backgroundColor: const Color(0xFF1B1B2F), // dark navy
        foregroundColor: Colors.white,
      ),
      backgroundColor: const Color(0xFFF5F5F0), // off-white

      // FutureBuilder<T> bridges async data and the widget tree. You hand it
      // a Future and a builder function; Flutter calls the builder every time
      // the Future's state changes (waiting → done or error). This is the
      // idiomatic Flutter alternative to useEffect + useState in React, or
      // manually managing a loading flag in imperative UI code.
      body: FutureBuilder<List<AppNotification>>(
        future: fetchNotifications(),

        // snapshot carries the current state of the Future:
        //   snapshot.connectionState — waiting, done, etc.
        //   snapshot.hasData        — true when the Future completed with a value
        //   snapshot.hasError       — true when the Future threw
        //   snapshot.data           — the value (null until done)
        //   snapshot.error          — the error object (null until thrown)
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          }

          if (snapshot.hasError) {
            return Center(
              child: Text(
                'Erro ao carregar notificações.\n${snapshot.error}',
                textAlign: TextAlign.center,
              ),
            );
          }

          final notifications = snapshot.data!;

          if (notifications.isEmpty) {
            return const Center(child: Text('Nenhuma notificação.'));
          }

          // ListView.builder is lazy — it only builds the items visible on
          // screen. For a small list this doesn't matter; for thousands of
          // items it's the difference between 60fps and a frozen UI.
          return ListView.builder(
            padding: const EdgeInsets.all(16),
            itemCount: notifications.length,
            itemBuilder: (context, index) {
              final n = notifications[index];
              return _NotificationCard(notification: n);
            },
          );
        },
      ),
    );
  }
}

// Private widget — underscore makes it invisible outside this file.
// Extracting the card into its own widget keeps the build method readable.
// Flutter encourages small, focused widgets over large build methods.
class _NotificationCard extends StatelessWidget {
  final AppNotification notification;

  const _NotificationCard({required this.notification});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      elevation: 0,
      color: Colors.white,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              notification.title,
              style: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w600,
                color: Color(0xFF1B1B2F),
              ),
            ),
            const SizedBox(height: 6),
            Text(
              notification.body,
              style: const TextStyle(fontSize: 14, color: Color(0xFF555555)),
            ),
          ],
        ),
      ),
    );
  }
}
