#!/bin/bash
# Detects the Windows host LAN IP via PowerShell and runs the Flutter app
# with the backend URL injected at compile time via --dart-define.
# Usage: ./run_dev.sh [extra flutter run args]

# Update this whenever your phone's IP or ADB port changes.
# Find it in: Settings > Developer options > Wireless debugging
PHONE_ADB=192.168.2.106:45481

echo "Connecting to phone via ADB ($PHONE_ADB)..."
adb connect "$PHONE_ADB"

WINDOWS_IP=$(powershell.exe -NoProfile -Command "Get-NetIPAddress -AddressFamily IPv4 | Where-Object { \$_.IPAddress -notmatch '^(127\.|169\.|172\.)' } | Select-Object -ExpandProperty IPAddress -First 1" | tr -d '\r\n')

if [ -z "$WINDOWS_IP" ]; then
  echo "ERROR: Could not detect Windows LAN IP. Are you connected to Wi-Fi or Ethernet?"
  exit 1
fi

echo "Windows LAN IP: $WINDOWS_IP"
echo "Starting Flutter with API_BASE_URL=http://$WINDOWS_IP:8080"

cd "$(dirname "$0")/mobile/atenciosamente_app"
flutter run --dart-define=API_BASE_URL=http://$WINDOWS_IP:8080 "$@"