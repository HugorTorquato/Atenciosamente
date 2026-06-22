# Dart Language Concepts — A Guide for C++ Developers

This document explains every Dart feature used in the Phase 0 mobile code,
always framed against what you already know from C++.

---

## Type system

### `final` vs `const`

```dart
final int id;               // runtime constant — set once at construction, never again
const String label = 'foo'; // compile-time constant
```

In C++ terms:
- `final` ≈ a `const` member variable — immutable after the constructor runs.
- `const` in Dart ≈ `constexpr` — the value must be known at compile time.

For data model fields (`id`, `title`, `body`, `createdAt` in `Notification`),
always use `final`. The object is constructed from JSON and never mutated.

---

### `dynamic` — the escape hatch

```dart
Map<String, dynamic> json
```

`dynamic` tells the type system "I don't know the type at compile time — trust me
at runtime." It is the equivalent of `void*` in C, or `std::any` in C++17.

You see it in JSON handling because Dart's `jsonDecode()` can't know whether a
JSON value is an `int`, `String`, `List`, or another `Map` until it reads the
bytes. The entire decoded tree is `dynamic`.

The rule: **narrow `dynamic` to a concrete type as soon as possible** using `as`:

```dart
json['id'] as int       // throws TypeError if the actual type isn't int
json['title'] as String // same
```

This is safer than a reinterpret_cast in C++ because it throws a clear, readable
`TypeError` rather than undefined behaviour.

---

### Null safety

Dart has built-in null safety (since Dart 2.12). Every type is non-nullable by default:

```dart
String title;    // cannot be null — compiler enforces this
String? title;   // nullable — the ? explicitly opts in to null
```

This is similar to using `std::optional<std::string>` everywhere, except Dart
enforces it at the language level. The compiler will refuse to compile code that
could dereference a null without a check.

In our `Notification` class all fields are non-nullable — the model assumes the
backend always sends complete JSON. If a field were optional (say, a subtitle
that some notifications omit), you'd write `String? subtitle`.

---

## Constructors

### Named parameters and `required`

```dart
const Notification({
  required this.id,
  required this.title,
  required this.body,
  required this.createdAt,
});
```

In C++ you'd write `Notification(int id, std::string title, ...)` and callers
depend on argument order. In Dart, wrapping parameters in `{}` makes them named:

```dart
// C++ style — positional, order-dependent
Notification(1, "Reunião às 15h", "Não se esqueça...", timestamp);

// Dart style — named, order-independent, self-documenting
Notification(id: 1, title: "Reunião às 15h", body: "Não se esqueça...", createdAt: timestamp);
```

`required` means the compiler rejects any call site that omits that argument —
no runtime null checks needed. `this.id` in the parameter list is shorthand for
assigning the parameter directly to the field — equivalent to
`Notification(int id) : id_(id) {}` in C++.

---

### `factory` constructors

```dart
factory Notification.fromJson(Map<String, dynamic> json) {
  return Notification(
    id: json['id'] as int,
    title: json['title'] as String,
    body: json['body'] as String,
    createdAt: DateTime.parse(json['created_at'] as String),
  );
}
```

Dart allows a class to have multiple *named* constructors. `Notification.fromJson`
is a second constructor invoked as `Notification.fromJson(someMap)`.

The `factory` keyword means the constructor body runs before an instance is
created, and it returns one explicitly. This is the standard Dart pattern for
deserializing JSON. Compare to a C++ static factory function:

```cpp
static Notification from_json(const nlohmann::json& j) {
    return Notification{j["id"], j["title"], j["body"], parse_time(j["created_at"])};
}
```

Same idea, different syntax. At Phase 1+ you'd use code generation (`json_serializable`)
to avoid writing `fromJson` by hand, but doing it manually now means you understand
what the generator produces.

---

## Access control — the underscore convention

Dart has no `private`, `protected`, or `public` keywords.
Access control works entirely by naming convention:

```dart
const String _baseUrl = '...';   // library-private — invisible outside this file
class _NotificationCard { ... }  // same — only usable in notifications_screen.dart
```

A leading `_` makes the identifier visible only within the same Dart *library*
(roughly: the same file). The compiler enforces this — using `_baseUrl` from
another file is a compile error, not just a warning.

For public identifiers, simply don't prefix with `_`.

---

## Async / await and `Future<T>`

This is the most important section if you're coming from C++.

### The mental model

Dart is single-threaded. There is one thread, one event loop. There are no
mutexes or thread pools for I/O — the OS handles I/O behind the scenes and posts
completion events back to the event loop.

```
┌────────────────────────────────────────────────┐
│               Dart Event Loop                  │
│                                                │
│  render frame → handle tap → render frame ...  │
│                      ↑                         │
│          network response posted here          │
└────────────────────────────────────────────────┘
```

When you `await` an HTTP request, the function is *suspended* and control returns
to the event loop. The UI keeps running — rendering frames, handling taps. When
the network response arrives, the event loop *resumes* your function exactly
where it was suspended.

### `Future<T>` vs `std::future<T>`

| C++ | Dart |
|---|---|
| `std::future<T>` | `Future<T>` |
| `std::promise<T>` | `Completer<T>` (rarely used directly) |
| `.get()` — blocks the calling thread | `await` — suspends the function, not the thread |
| callback-based async | `async`/`await` (reads like synchronous code) |

```dart
// fetchNotifications() is suspended here while the network call runs.
// The Flutter engine keeps rendering frames — the user sees the spinner.
final response = await http.get(Uri.parse('$_baseUrl/notifications'));
// Execution resumes here when the response is ready.
```

A function must be marked `async` to use `await` inside it. An `async` function
always returns `Future<T>` — if you write `return someT`, the compiler wraps it
automatically.

### Error handling through Futures

Exceptions thrown inside an `async` function propagate through the `Future`.
The `Future` carries either a value or an error — never both. The caller handles
errors by:

1. `try`/`catch` around an `await`
2. Passing the `Future` to `FutureBuilder`, which surfaces the error in `snapshot.error`

In `fetchNotifications()` we throw when the HTTP status is not 200. `FutureBuilder`
on the widget side catches that and renders the error message.

---

## Collections and functional-style operations

```dart
return body
    .map((item) => Notification.fromJson(item as Map<String, dynamic>))
    .toList();
```

`body` is `List<dynamic>`. `.map()` is like `std::transform` — applies a function
to each element and returns a lazy iterable. `.toList()` materialises it into a
concrete `List<Notification>`.

The `(item) => expr` syntax is a lambda, equivalent to `[](auto item){ return expr; }`
in C++. Dart also has multi-line lambdas:

```dart
.map((item) {
  final map = item as Map<String, dynamic>;
  return Notification.fromJson(map);
})
```

---

## Import system

```dart
import 'dart:convert';                      // Dart standard library
import 'package:http/http.dart' as http;    // third-party package (pub.dev)
import '../models/notification.dart';       // relative file import
```

| Import style | Equivalent in C++ |
|---|---|
| `dart:convert` | Standard library header — `<sstream>`, `<string>` |
| `package:http/...` | vcpkg dependency — `nlohmann/json`, `crow` |
| Relative path | Local header — `#include "../models/notification.hpp"` |

The `as http` alias namespaces all exports under `http.` — so `http.get()`,
`http.Response`, etc. Without it, the unqualified `get` could collide with
other names in scope. Same reasoning as `namespace` aliases in C++.
