---
name: frontend-add-model
description: >-
  Add a Dart model class to the Atenciosamente Flutter app that mirrors a
  backend JSON shape, with a fromJson factory. Use when the app needs to consume
  a new backend resource. Frontend-only; keeps the app deliberately minimal.
---

# Add a Flutter model

A model is a plain Dart class in `lib/models/` whose fields mirror the backend JSON, plus a
`fromJson` factory that parses one JSON object. The template is
`lib/models/notification.dart` (`AppNotification`): `final` fields, a `const` constructor
with `required` named params, and a `factory ... .fromJson(Map<String, dynamic> json)`.

## Procedure

1. **Match the backend shape.** Read the matching C++ side first —
   `backend/src/domain/notification.hpp` and `notification_json.hpp` — so field
   names and JSON keys line up exactly (e.g. backend emits `created_at`; the Dart field is
   `createdAt`, parsed from `json['created_at']`).
2. **Create** `lib/models/<entity>.dart` with one class:
   - `final` typed fields; `const <Class>({ required this.x, ... })`.
   - `factory <Class>.fromJson(Map<String, dynamic> json)` casting each value
     (`as int`, `as String`) and parsing dates with `DateTime.parse(json['...'] as String)`.
   - Add a `toJson()` **only if** the app sends this entity to the backend (e.g. a POST body);
     don't add it speculatively.
3. **Comment for learning** — the existing models/screens carry explanatory comments for a
   C++ developer; match that density (what a `factory` is, why `fromJson` is separate from
   the constructor).
4. **Analyze & test:** `flutter analyze`; if the model has non-trivial parsing, add a small
   `test/` case. A pure model rarely needs a widget test.
5. **Commit** — one line, `Mobile (Feat): add <Entity> model` (no body, no trailers).

Keep it minimal: a data class with `fromJson`. No code generation, no packages
(`json_serializable`, `freezed`) unless the developer asks — this app stays simple.
