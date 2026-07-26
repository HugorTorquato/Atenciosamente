# Open this project in VS Code using WSL

This document explains how to open the Atenciosamente project in VS Code using WSL.

## Prerequisites
- Windows with WSL2 enabled and an Ubuntu (or other) distro installed.
- VS Code installed on Windows.
- The `Remote - WSL` extension installed in VS Code.

## Quick steps (recommended)

1. Open Windows Terminal, PowerShell, or Command Prompt.
2. Start your WSL distro and change to the project folder:

```bash
wsl -d Ubuntu
cd ~/projects/Atenciosamente
```

3. From inside WSL, open the folder in VS Code (opens a WSL window):

```bash
code .
```

This will launch VS Code connected to the WSL environment and open the repository folder.

## Alternative: From Windows VS Code UI

1. Open VS Code on Windows.
2. Click the green remote indicator in the lower-left corner and choose `Remote-WSL: New Window`.
3. In that WSL window, choose `File → Open Folder...` and navigate to `/home/<your-username>/projects/Atenciosamente` (for this repo: `/home/torquato/projects/Atenciosamente`).

## Recommended extensions & settings
- Install `Remote - WSL` (required) and the extensions listed in the repo's recommendations (if present).
- When prompted, allow installing extensions into the WSL window, not into the local Windows host.

## Common commands you may run inside the WSL VS Code terminal

Start dev containers (if using Docker Compose):

```bash
docker compose up -d
```

Build the backend (inside the container or WSL dev environment):

```bash
cmake --preset=dev
cmake --build --preset=dev
ctest --preset=dev
```

## Troubleshooting
- If `code .` is not found, ensure you installed the `code` CLI from the VS Code command palette: `Shell Command: Install 'code' command in PATH` (or install the Remote - WSL extension which exposes `code` in WSL).
- If permissions prevent opening the folder, check that your WSL user owns the project files: `sudo chown -R $(id -u):$(id -g) ~/projects/Atenciosamente`.
- If Docker commands require sudo, either run them with `sudo` or add your user to the `docker` group.

## File location
Saved as: Documentation/OPEN_PROJECT_IN_WSL.md

---
If you want, I can commit this file and create a short entry in `Documentation/README.md` linking to it.
