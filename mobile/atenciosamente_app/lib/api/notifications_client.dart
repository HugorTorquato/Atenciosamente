import 'dart:convert';
import 'package:http/http.dart' as http;
import '../models/notification.dart';

// Injected at run time via --dart-define=API_BASE_URL=http://<host-ip>:8080.
// Use run_dev.sh at the repo root — it detects the Windows LAN IP automatically.
// Fallback to localhost for CI or emulator runs without the script.
const String _baseUrl = String.fromEnvironment(
  'API_BASE_URL',
  defaultValue: 'http://localhost:8080',
);

// Future<T> is Dart equivalent of std::future<T> — a value that isn't ready
// yet. The caller can await it without blocking the UI thread.
Future<List<AppNotification>> fetchNotifications() async {
  // http.get() sends a GET request and returns a Future<Response>. The await
  // suspends this function until the response arrives, then resumes — no
  // threads, no callbacks, no blocking. Dart's event loop handles the rest.
  final response = await http.get(Uri.parse(_baseUrl + '/notifications'));

  if (response.statusCode != 200) {
    // Throwing here surfaces the error to whoever awaits fetchNotifications().
    // FutureBuilder on the widget side catches this and shows an error state.
    throw Exception('Failed to load notifications (' + response.statusCode.toString() + ')');
  }

  // jsonDecode returns dynamic — the JSON structure is unknown at compile time.
  // We cast to List<dynamic> because we know the backend returns a JSON array.
  final List<dynamic> body = jsonDecode(response.body) as List<dynamic>;

  // Map each raw JSON object (Map<String, dynamic>) through fromJson and
  // collect the results into a typed List<AppNotification>.
  return body
      .map((item) => AppNotification.fromJson(item as Map<String, dynamic>))
      .toList();
}
