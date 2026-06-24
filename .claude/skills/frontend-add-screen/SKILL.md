---
name: frontend-add-screen
description: >-
  Add a full-screen widget to the Atenciosamente Flutter app and wire its route.
  Use when adding a new page (detail view, create form, etc.). Frontend-only;
  keeps the app deliberately minimal.
---

# Add a Flutter screen

A screen is a full-page widget in `lib/screens/`, one per route, returning a `Scaffold`. The
template is `lib/screens/notifications_screen.dart`: a `StatelessWidget` with a `Scaffold`
(`AppBar` + `body`), and — for async data — a `FutureBuilder` handling waiting / error /
data states. Routing is currently via `MaterialApp(home: ...)` in `lib/main.dart` (no named
routes yet).

## Procedure

1. **Choose the widget type:**
   - `StatelessWidget` — renders purely from constructor args / a Future (use `FutureBuilder`
     for async). This is the default; the existing screen uses it.
   - `StatefulWidget` — only when the screen has mutable state that changes over time (form
     fields, toggles). Explain *why* state is needed when you reach for it.
2. **Create** `lib/screens/<name>_screen.dart` returning a `Scaffold` (`AppBar` with title,
   themed `body`). Reuse the app's colors (dark navy `0xFF1B1B2F`, off-white `0xFFF5F5F0`)
   and extract repeated sub-UI into small private `_Widget` classes, as the existing screen
   does — small focused widgets over large `build` methods.
3. **Fetch data** (if any) through `lib/api/` functions, not inline `http` calls, so the
   base-URL/`--dart-define` wiring stays in one place. Handle waiting / error / empty in the
   `FutureBuilder`, like `notifications_screen.dart`.
4. **Wire the route:**
   - One entry point → set `home:` (or push it) in `lib/main.dart`.
   - Multiple routes → introduce named routes (`routes: { '/detail': ... }`) and navigate
     with `Navigator.pushNamed`. Introducing named routing is a small architecture step —
     mention it to the developer.
5. **Comment for learning** — match the heavy explanatory style of the existing screen
   (what `Scaffold` / `FutureBuilder` / `ListView.builder` do).
6. **Analyze & test:** `flutter analyze`; add/extend a widget test in `test/` for the new
   screen if it has real logic.
7. **Commit** — one line, `Mobile (Feat): add <name> screen` (no body, no trailers).

Keep it minimal — no navigation packages or state-management libraries unless the developer
asks. The app exists to exercise the backend.
