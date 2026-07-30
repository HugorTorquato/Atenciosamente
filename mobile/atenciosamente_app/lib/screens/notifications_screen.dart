import 'package:flutter/material.dart';
import '../api/notifications_client.dart';
import '../models/notification.dart';
import 'create_notification_screen.dart';

// This screen used to be a StatelessWidget: the Future it displayed was
// created fresh inline inside build() (`future: fetchNotifications()`), and
// FutureBuilder managed its lifecycle internally — the screen itself held no
// state of its own. That stopped being enough once "create a notification"
// entered the picture: after CreateNotificationScreen pops back, this screen
// needs to *refetch* so the new item shows up in the list. A StatelessWidget
// has no hook for "do something in response to an event, later, after the
// widget already built" — build() only runs from outside triggers (parent
// rebuilds), it can't ask itself to run again. StatefulWidget's State object
// solves this: it can hold the Future in a field and reassign it via
// setState(), which tells Flutter "rebuild me, something changed." This is
// still the plain built-in State/setState — not a state-management package,
// just the mechanism Flutter ships with for exactly this kind of case.
class NotificationsScreen extends StatefulWidget {
  const NotificationsScreen({super.key});

  @override
  State<NotificationsScreen> createState() => _NotificationsScreenState();
}

class _NotificationsScreenState extends State<NotificationsScreen> {
  // Holding the Future in State (rather than calling fetchNotifications()
  // directly inside build()) means it's only created once per fetch, not on
  // every rebuild — and it gives us a field we can reassign to trigger a
  // fresh fetch on demand.
  late Future<List<AppNotification>> _notificationsFuture;

  @override
  void initState() {
    // initState() runs exactly once, when the State is first created —
    // the natural place to kick off the initial fetch. Compare to a
    // constructor in C++, except widget rebuilds don't re-run it.
    super.initState();
    _notificationsFuture = fetchNotifications();
  }

  Future<void> _openCreateScreen() async {
    // Navigator.push returns a Future that resolves with whatever value the
    // pushed screen passes to Navigator.pop(). CreateNotificationScreen pops
    // with `true` on a successful create, and nothing (null) if the user
    // just backs out — so we only refetch in the success case.
    final created = await Navigator.push<bool>(
      context,
      MaterialPageRoute(builder: (context) => const CreateNotificationScreen()),
    );

    if (created == true) {
      // Reassigning the Future inside setState() is what tells Flutter to
      // rebuild: FutureBuilder below sees a new Future instance and starts
      // watching it from its "waiting" state again.
      setState(() {
        _notificationsFuture = fetchNotifications();
      });
    }
  }

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
        future: _notificationsFuture,

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

      // The "+" FAB is the idiomatic Material affordance for "create a new
      // item in this list". Tapping it pushes CreateNotificationScreen and
      // awaits its result in _openCreateScreen().
      floatingActionButton: FloatingActionButton(
        onPressed: _openCreateScreen,
        backgroundColor: const Color(0xFF1B1B2F),
        foregroundColor: Colors.white,
        child: const Icon(Icons.add),
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
