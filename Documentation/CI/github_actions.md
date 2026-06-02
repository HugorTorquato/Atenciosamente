# GitHub Actions CI

This project has two independent CI pipelines — one per component. They share
the same structural design but run on entirely different toolchains.

---

## Overview

| File | Triggers on | What it does |
|---|---|---|
| `backend-ci.yml` | changes inside `backend/` | CMake configure → build → Catch2 tests |
| `mobile-ci.yml` | changes inside `mobile/` | Flutter analyze → debug APK build |

Because each workflow declares a `paths:` filter, the two pipelines are
completely isolated: a Flutter commit never triggers the C++ build, and a
backend commit never triggers a Flutter build. This matters as both builds are
slow on a cold run (~3-5 min each).

---

## Design principles shared by both workflows

### Path filtering

```yaml
on:
  push:
    branches: [main]
    paths:
      - 'mobile/**'               # any file inside the mobile directory
      - '.github/workflows/mobile-ci.yml'  # the workflow file itself
```

The workflow file is included in the paths filter so that changes to CI
configuration (adding a step, bumping a dependency) also trigger a run. Without
this, you could break CI and not notice until you next touch mobile source.

### Concurrency cancellation

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.ref }}
  cancel-in-progress: true
```

If you push two commits in quick succession, GitHub Actions will cancel the
first run and keep only the second. This avoids wasting runner minutes on
intermediate commits that are already superseded.

- `head_ref` — the PR branch name; stable across force-pushes
- `ref` — fallback for direct push events (e.g. `refs/heads/main`)

### Least-privilege permissions

```yaml
permissions:
  contents: read
```

The job only needs to read the repository. Restricting permissions is a
security best practice — if a dependency in the build is compromised, it
cannot write to your repo or create releases.

---

## Backend CI (`backend-ci.yml`)

Runs on `ubuntu-24.04`. Steps in order:

1. **Checkout** — `actions/checkout@v4` clones the repo at the triggering commit.
2. **Install build tools** — apt installs ninja, pkg-config, libssl-dev, zip/unzip,
   bison/flex/autoconf (required by some vcpkg ports like libpq).
   Sets `gcc-13` and `g++-13` as the default compiler via `update-alternatives`.
3. **Install vcpkg** — clones vcpkg to `/opt/vcpkg` and bootstraps it.
   Package binaries are cached via `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite`.
   Cold run: vcpkg compiles from source (~3 min). Warm run: restores binaries (~seconds).
4. **Configure** — `cmake --preset ci` (Release, BUILD_TESTING=ON).
   This is where vcpkg manifest install runs and pulls all C++ dependencies.
5. **Build** — `cmake --build --preset ci`.
6. **Test** — `ctest --preset ci --output-junit test-results.xml`.
   A test failure exits non-zero and fails the job.
7. **Upload test results** — saves `test-results.xml` as a GitHub Actions artifact,
   even if tests failed (`if: always()`). You can download it from the Actions tab.

---

## Mobile CI (`mobile-ci.yml`)

Runs on `ubuntu-24.04`. Steps in order:

### 1. Checkout

Same as backend — `actions/checkout@v4`.

### 2. Set up Java 17

```yaml
- uses: actions/setup-java@v4
  with:
    distribution: temurin
    java-version: '17'
```

The Android build toolchain — Gradle, sdkmanager, aapt2, d8 — is a JVM program.
Flutter needs Java to compile the Android side of the app. Temurin is the Eclipse
Adoptium distribution (free, LTS, widely used). The `ubuntu-24.04` runner ships
with Java 21 by default but Flutter's Gradle plugin requires Java 17.

### 3. Set up Flutter

```yaml
- uses: subosito/flutter-action@v2
  with:
    channel: stable
    cache: true
```

`subosito/flutter-action` is the standard community action for Flutter CI. It:
- Downloads the Flutter SDK at the latest `stable` channel release
- Adds `flutter` and `dart` to PATH
- Caches the SDK between runs (saves ~1-2 minutes on warm runs)

The `ubuntu-24.04` runner already has the Android SDK pre-installed at
`$ANDROID_SDK_ROOT`. No separate Android Studio or cmdline-tools install needed.

### 4. Accept Android SDK licenses

```yaml
run: yes | $ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager --licenses || true
```

Android's build toolchain requires all SDK licenses to be explicitly accepted
before it will download or use any SDK components. On a fresh CI runner they are
not accepted. `yes |` pipes a stream of "y" responses to every prompt.
`|| true` prevents the step from failing if some licenses were already accepted.

**Why this step exists:** Without it, `flutter build apk` would fail with
`"License for package Android SDK Build-Tools XX not accepted"` even though
the SDK is installed — the license file is missing.

### 5. Install dependencies

```yaml
run: flutter pub get
working-directory: mobile/atenciosamente_app
```

Reads `pubspec.yaml` + `pubspec.lock` and fetches all packages.
Equivalent to `npm ci` in Node.js: uses the exact locked versions, fails if
`pubspec.lock` is out of sync with `pubspec.yaml`. This is the fastest sanity
check that your dependency declarations are consistent.

### 6. Analyze

```yaml
run: flutter analyze
working-directory: mobile/atenciosamente_app
```

Runs the Dart static analyzer across every `.dart` file in the project.

Catches without a device or emulator:
- Type errors (e.g. passing `String` where `int` is expected)
- Null-safety violations (accessing `.field` on a nullable value without a `?` check)
- Unused imports and variables
- Deprecated API usage
- Code-style issues flagged by the `analysis_options.yaml` rules

This is the fastest and most valuable check in mobile CI. It runs in seconds
and catches the majority of mistakes.

### 7. Build debug APK

```yaml
run: flutter build apk --debug
working-directory: mobile/atenciosamente_app
```

Compiles the full app to a `.apk` file. `--debug` skips release signing — no
keystore or signing credentials are required in CI.

Why this step is worth the extra ~2 minutes:

- `flutter analyze` checks Dart code but not the Android layer
- This step compiles Kotlin/Java glue code, runs Gradle, links native plugins,
  and validates `AndroidManifest.xml` — errors there are invisible to the analyzer
- It proves the app would install on a real device from a clean machine

The resulting APK is not uploaded as an artifact (no need at Phase 0).

---

## Reading CI results

Go to your GitHub repository → **Actions** tab.

- Green checkmark = all steps passed
- Red X = at least one step failed — click into the run, then the job, then the
  failing step to read the full log
- Yellow circle = run in progress
- Grey circle = run was cancelled (usually by the concurrency policy)

For the backend, a failed **Test** step will have a `test-results.xml` artifact
available for download under **Artifacts** at the bottom of the run page.

---

## Test file hygiene

`flutter create` generates `test/widget_test.dart` as a smoke test for the
default counter app. When you replace `main.dart` with your own app, this file
becomes stale — it still references `MyApp` and counter-specific widgets that no
longer exist.

**Symptom:** `flutter analyze` fails with:

```
error • The name 'MyApp' isn't a class • test/widget_test.dart:16:35 • creation_with_non_type
```

**Why `flutter analyze` catches this:** the analyzer checks `test/` alongside
`lib/`. The test file imports `main.dart` and references `MyApp()`, which no
longer exists after the rename to `AtenciosamenteApp`.

**Fix applied (Phase 0):** the generated test was replaced with a minimal
placeholder that compiles and passes until real widget tests are written:

```dart
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('placeholder — replace with real widget tests in a future phase', () {
    expect(1 + 1, 2);
  });
}
```

**Rule going forward:** any time you rename or remove a top-level widget, check
`test/widget_test.dart` (and any other test files) for stale references before
pushing.

---

## What CI does NOT do

- **Run on a device or emulator** — no device tests (widget tests, integration
  tests). Adding these is a future phase task.
- **Build a release APK** — no signing keys are configured. Debug-only for now.
- **Deploy** — no CD pipeline. Manual `flutter run` to your phone for now.
- **Check the backend ↔ mobile contract** — the two pipelines are independent.
  If the backend changes its JSON shape and the Flutter model doesn't follow,
  CI won't catch it. This is a known limitation at Phase 0.
