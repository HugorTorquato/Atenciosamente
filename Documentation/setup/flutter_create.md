# Flutter Project Scaffold — `flutter create` Explained

## The command

```bash
flutter create \
  --org br.com.atenciosamente \
  --project-name atenciosamente_app \
  --platforms android \
  mobile/atenciosamente_app
```

**`--org br.com.atenciosamente`** — reverse-domain identifier, exactly like Java package names.
Android uses this as the app's global ID on the Play Store and device:
`br.com.atenciosamente.atenciosamente_app`. Baked into `AndroidManifest.xml` and Gradle
build files — painful to change later. Pick it once.

**`--project-name atenciosamente_app`** — the Dart package name. Dart convention is
`lowercase_with_underscores` (no hyphens, no camelCase). Becomes the name in `pubspec.yaml`
and in `import` statements inside your own code.

**`--platforms android`** — without this flag Flutter also generates iOS, web, Windows,
Linux, and macOS directories. Adding iOS later is one command.

---

## Generated file map

### Files you'll edit constantly

| File | Purpose |
|---|---|
| `lib/main.dart` | Entry point — `void main()`. Same concept as `main.cpp`. |
| `pubspec.yaml` | `CMakeLists.txt` + `vcpkg.json` combined. Declares app name, version, SDK constraints, dependencies. |

### Files you'll edit once or rarely

| File | Purpose |
|---|---|
| `android/app/src/main/AndroidManifest.xml` | App permissions (INTERNET, camera, etc.), display name, launcher icon. |
| `android/app/build.gradle.kts` | Android build config. Gradle is to Android what CMake is to C++. Edit when changing `minSdkVersion` or adding native Android deps. |
| `analysis_options.yaml` | Dart linter config — equivalent to `.clang-tidy`. Default is sensible; leave it alone. |

### Files you never touch

| File | Why it exists |
|---|---|
| `android/app/src/main/kotlin/.../MainActivity.kt` | The single Android Activity that hosts Flutter. 5 lines, never changes unless you need platform channels (Phase 4+). |
| `pubspec.lock` | Locked dependency versions — same concept as vcpkg's `builtin-baseline`. Commit it; never hand-edit it. |
| `android/gradlew` | Gradle wrapper — Flutter calls this internally to build the APK. |
| `android/gradle/wrapper/gradle-wrapper.properties` | Pins the Gradle version. |
| `.dart_tool/` | Flutter's internal resolution cache — equivalent to `build/`. Gitignored. |
| `.metadata` | Records which Flutter version created the project. Used by `flutter upgrade`. |

---

## Directory layout — day-to-day view

```
mobile/atenciosamente_app/
├── pubspec.yaml                    ← dependencies go here
├── lib/
│   └── main.dart                   ← app entry point
├── test/
│   └── widget_test.dart            ← widget tests
└── android/
    └── app/
        └── src/main/
            ├── AndroidManifest.xml ← add INTERNET permission here
            └── kotlin/.../
                └── MainActivity.kt ← never touch
```

## Target layout after Phase 0

```
lib/
├── main.dart
├── api/
│   └── notifications_client.dart   ← HTTP client
├── models/
│   └── notification.dart           ← JSON model
└── screens/
    └── notifications_screen.dart   ← FutureBuilder + ListView
```

---

## Adding dependencies — `pubspec.yaml` and `flutter pub get`

### Adding the `http` package

`pubspec.yaml` after editing:

```yaml
dependencies:
  flutter:
    sdk: flutter
  http: ^1.2.2        ← added
  cupertino_icons: ^1.0.8
```

`^1.2.2` means "any version compatible with 1.2.2" — i.e. `>=1.2.2 <2.0.0`.
Dart's pub tool picks the latest version that satisfies the constraint.

After editing `pubspec.yaml`, run:

```bash
flutter pub get
```

This is the equivalent of `cmake --preset=dev` resolving vcpkg dependencies — it
downloads packages and writes the exact resolved versions to `pubspec.lock`.

### Reading `flutter pub get` output

```
+ http 1.6.0           ← newly added (1.6.0 is the latest satisfying ^1.2.2)
+ http_parser 4.1.2    ← transitive dependency pulled in by http
+ typed_data 1.4.0     ← transitive dependency
+ web 1.1.1            ← transitive dependency
Changed 4 dependencies!
15 packages have newer versions incompatible with dependency constraints.
```

**`+` prefix** — newly installed package.
**No prefix** — already present, shown for information.
**Transitive dependencies** — packages that `http` itself depends on. You don't declare
them; pub resolves the full graph automatically, same as vcpkg.

**"15 packages have newer versions incompatible with dependency constraints"** — normal.
It means those packages have released a new major version that would require changing the
version constraint (e.g. `^5.0.0 → ^6.0.0`). This is not an error; it's informational.
Run `flutter pub outdated` to see the full list when you want to upgrade.

### `INTERNET` permission in `AndroidManifest.xml`

Android apps must declare permissions upfront. Without this line, HTTP calls fail silently
at runtime:

```xml
<uses-permission android:name="android.permission.INTERNET"/>
```

Added above the `<application>` tag in
`android/app/src/main/AndroidManifest.xml`.
