# Flutter SDK — Installation & Setup

## Installation (WSL2)

**Why not `snap install flutter`:** snap has reliability issues inside WSL2 — daemons
don't start cleanly. Use the official Linux tarball instead.

### Prerequisites

```bash
# Java 17 is required by Android SDK tools (sdkmanager, etc.)
sudo apt install openjdk-17-jdk unzip adb -y

echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
```

### Flutter SDK

```bash
cd ~
wget -q --show-progress \
  https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.32.0-stable.tar.xz

tar xf flutter_linux_3.32.0-stable.tar.xz

echo 'export PATH="$PATH:$HOME/flutter/bin"' >> ~/.bashrc
source ~/.bashrc

flutter --version
```

**SDK location:** `~/flutter/` (toolchain, not inside the project repo)

**Stable version installed:** 3.32.0

### Android SDK (without Android Studio)

```bash
mkdir -p ~/Android/Sdk/cmdline-tools
cd ~/Android/Sdk/cmdline-tools

wget -q --show-progress \
  https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-11076708_latest.zip
mv cmdline-tools latest

echo 'export ANDROID_HOME=~/Android/Sdk' >> ~/.bashrc
echo 'export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools' >> ~/.bashrc
source ~/.bashrc

# Install required components (Android 13 = API level 33)
sdkmanager "platform-tools" "platforms;android-33" "build-tools;33.0.1"

# Accept Flutter's Android licenses
flutter doctor --android-licenses
```

---

## Target device

| Field | Value |
|---|---|
| Device | Samsung Galaxy S20 FE |
| Android version | 13 (One UI 5.1) |
| ADB mode | Wireless (Android 11+ feature) |

---

## Wireless ADB setup (one-time, per device)

Android 13 supports wireless debugging natively — no USB cable needed after pairing.

### On the phone

1. **Enable Developer Options:** Settings → About phone → tap *Build number* 7 times.
2. **Enable wireless debugging:** Settings → Developer options → Wireless debugging → toggle ON.
3. **Pair with a code:** inside Wireless debugging → *Pair device with pairing code* — note the **IP:port** and **6-digit code** shown.

### In WSL2

```bash
# Pair once (use the pairing port and code from the phone screen above)
adb pair <phone-IP>:<pairing-port>
# enter the 6-digit code when prompted

# Connect (use the main wireless debugging IP:port shown on the phone — different port from pairing)
adb connect <phone-IP>:<main-port>

# Verify
adb devices
```

After pairing, reconnect with `adb connect <phone-IP>:<main-port>` whenever you start a session.

---

## flutter doctor expected output

Target state (Android real device, no Android Studio):

```
[✓] Flutter
[✓] Android toolchain - develop for Android devices (Android SDK version 33.0.1)
[✗] Chrome                ← irrelevant, web target
[✗] Linux toolchain       ← irrelevant, desktop target
[!] Android Studio        ← irrelevant, we use VS Code
[✓] Connected device (2 available)
[✓] Network resources
```

The `✗` entries for Chrome, Linux toolchain, and Android Studio do not block Android development.

---

## Backend URL for real device

The phone is on the same LAN as the dev machine. Use the WSL2 host LAN IP:

```
http://172.22.238.44:8080
```

> This IP can change if the network changes. For a more permanent solution (Phase 1+),
> consider hardcoding the dev machine's router-assigned IP or using mDNS.

| Target | Base URL |
|---|---|
| Real device (LAN) | `http://172.22.238.44:8080` |
| Android emulator | `http://10.0.2.2:8080` |
| iOS simulator | `http://localhost:8080` |
