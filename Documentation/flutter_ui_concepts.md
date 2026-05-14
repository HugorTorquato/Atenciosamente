# Flutter UI Concepts

This document explains the Flutter widget system used in Phase 0:
`StatelessWidget`, `FutureBuilder`, the widget tree, and how they connect.

---

## The widget tree

In Flutter, **everything is a widget**. The screen is a widget. The app bar is a
widget. The padding around a card is a widget. A widget is a Dart class that
describes a piece of UI — its layout, style, and children.

The UI is a tree of widgets, exactly like the DOM in a browser, or a scene graph
in a game engine. Flutter walks this tree on every frame and produces pixels.

```
AtenciosamenteApp  (MaterialApp — root, sets theme and navigation)
└── NotificationsScreen  (Scaffold — screen skeleton)
    ├── AppBar  (top bar with title)
    └── FutureBuilder  (manages the async data lifecycle)
        ├── CircularProgressIndicator  (shown while waiting)
        ├── Text (error message)       (shown on failure)
        └── ListView.builder           (shown on success)
            ├── _NotificationCard  (item 0)
            ├── _NotificationCard  (item 1)
            └── _NotificationCard  (item 2)
```

Each `build()` method returns the subtree rooted at that widget. Flutter calls
`build()` whenever it needs to re-render a widget — which is very often and very
cheap by design.

---

## `StatelessWidget` vs `StatefulWidget`

### `StatelessWidget`

A widget with no mutable state. Its `build()` method is a pure function of its
constructor arguments — given the same inputs it always produces the same output.
Flutter can call `build()` as often as it likes without side effects.

```dart
class NotificationsScreen extends StatelessWidget {
  const NotificationsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    // No mutable fields — this is pure rendering logic.
    return Scaffold(...);
  }
}
```

Use `StatelessWidget` when the widget doesn't need to react to changes over time.

### `StatefulWidget`

A widget that owns mutable state and can re-render itself when that state changes.
It comes in two parts:

1. The widget class itself — holds constructor arguments (immutable, like a config).
2. A paired `State<T>` class — holds the mutable data and the `build()` method.

```dart
class Counter extends StatefulWidget {
  const Counter({super.key});
  @override
  State<Counter> createState() => _CounterState();
}

class _CounterState extends State<Counter> {
  int _count = 0;  // mutable state lives here

  void _increment() {
    setState(() { _count++; });
    // setState() tells Flutter: "re-call build() for this widget".
  }

  @override
  Widget build(BuildContext context) {
    return Text('$_count');
  }
}
```

`setState()` is the key — it schedules a rebuild. Without it, changing `_count`
does nothing visible. Think of it like calling `notifyDataSetChanged()` in Android,
or `invalidate()` in a canvas-based UI.

**Why `NotificationsScreen` is `StatelessWidget` even though it fetches data:**
`FutureBuilder` manages the async lifecycle internally — it has its own state.
Our screen just hands it a `Future` and a builder function; we don't need to
track loading/error/data ourselves.

---

## Common layout and display widgets

| Widget | Purpose | C++ / Android analogy |
|---|---|---|
| `Scaffold` | Screen skeleton: app bar + body + FAB + drawer | `Activity` layout |
| `AppBar` | Top navigation bar | `ActionBar` / `Toolbar` |
| `Center` | Centers its single child | `gravity="center"` |
| `Column` | Vertical list of children | `LinearLayout vertical` |
| `Row` | Horizontal list of children | `LinearLayout horizontal` |
| `Padding` | Adds space around its child | `android:padding` |
| `SizedBox` | Fixed-size spacing or a sized box | Spacer / `margin` |
| `Card` | Material card with shadow | `CardView` |
| `Text` | Renders a string | `TextView` |
| `ListView.builder` | Lazy scrollable list | `RecyclerView` |
| `CircularProgressIndicator` | Spinner | `ProgressBar` |

### Layout model in one sentence

Flutter uses a **constraint-based layout**: a parent passes constraints (min/max
width and height) down to its children; children choose their size within those
constraints and report back up. This is deterministic and fast — no measurement
passes like in Android's `onMeasure`.

---

## `FutureBuilder` — the async-to-widget bridge

`FutureBuilder<T>` is the standard Flutter answer to the question: *"how do I
show a loading spinner while data loads, then show the data, then show an error
if it fails?"*

```dart
FutureBuilder<List<Notification>>(
  future: fetchNotifications(),   // the async operation
  builder: (context, snapshot) { // called every time the Future's state changes
    if (snapshot.connectionState == ConnectionState.waiting) {
      return CircularProgressIndicator();  // still loading
    }
    if (snapshot.hasError) {
      return Text('Error: ${snapshot.error}');  // failed
    }
    final notifications = snapshot.data!;       // succeeded
    return ListView.builder(...);
  },
)
```

### The `snapshot` object

`AsyncSnapshot<T>` carries the full state of the `Future` at any point in time:

| Property | Type | Meaning |
|---|---|---|
| `snapshot.connectionState` | `ConnectionState` | `waiting`, `active`, `done` |
| `snapshot.hasData` | `bool` | `true` when data is available |
| `snapshot.hasError` | `bool` | `true` when the Future threw |
| `snapshot.data` | `T?` | The value (null until done without error) |
| `snapshot.error` | `Object?` | The thrown object (null until an error occurs) |

### The lifecycle

```
fetchNotifications() called
        │
        ▼
ConnectionState.waiting  →  builder returns spinner
        │
        ├── (success) ConnectionState.done, snapshot.hasData = true
        │         →  builder returns ListView
        │
        └── (error)  ConnectionState.done, snapshot.hasError = true
                  →  builder returns error Text
```

`FutureBuilder` calls `builder` at least twice: once immediately with `waiting`,
once again when the `Future` resolves. Each call can return a completely different
widget — Flutter diffs the tree and updates only what changed.

### The `!` operator and null safety

```dart
final notifications = snapshot.data!;
```

`snapshot.data` is `List<Notification>?` — it could be null. The `!` is a
null-assertion: "I am certain this is not null right now." It is safe here
because we only reach this line after checking `!snapshot.hasError`, which
means the Future completed successfully, which means `snapshot.data` is set.

If you're wrong about `!`, Dart throws `Null check operator used on a null value`
at runtime — clearer than a segfault.

---

## `ListView.builder` — lazy rendering

```dart
ListView.builder(
  itemCount: notifications.length,
  itemBuilder: (context, index) {
    final n = notifications[index];
    return _NotificationCard(notification: n);
  },
)
```

`ListView.builder` is lazy — it calls `itemBuilder` only for the items currently
visible on screen. For 3 notifications this makes no difference. For 10,000 it's
the difference between 60 fps and a frozen app.

Compare to `std::views::lazy` in C++23, or a generator in Python — it produces
elements on demand rather than materialising the whole list at once.

---

## Widget keys — `{super.key}`

```dart
const NotificationsScreen({super.key});
```

You'll see `{super.key}` on every widget constructor. Keys are how Flutter
identifies widget instances across rebuilds — it uses them to match old widgets
to new ones in the diff algorithm (similar to React's `key` prop in lists).

Passing `super.key` up to the parent class lets callers assign a key externally
if needed. For Phase 0 you don't need to think about this beyond "it's required
boilerplate."

---

## `BuildContext`

```dart
Widget build(BuildContext context) { ... }
```

`BuildContext` is a handle to a widget's position in the tree. Flutter uses it
to look up inherited data (like the current `Theme`, `MediaQuery` for screen
size, or `Navigator` for routing). You'll see it passed to helper functions that
need to look up tree-level data.

For now, think of it as a required parameter that opens the door to the tree's
ambient data.

---

## How the four Phase 0 files connect

```
main.dart
  └── AtenciosamenteApp (MaterialApp)
        └── NotificationsScreen (home:)
              └── FutureBuilder<List<Notification>>
                    future: fetchNotifications()  ← notifications_client.dart
                                                      calls http.get()
                                                      calls Notification.fromJson()
                                                          ← notification.dart
                    builder: returns ListView or spinner or error
```

Data flows in one direction: the client fetches, the model parses, the screen
renders. There is no shared mutable state — `fetchNotifications()` returns a new
list every time it's called.
