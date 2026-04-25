# Docker / Dev Container Command Reference

All commands run from `~/projects/Atenciosamente/` unless noted.

## Compose lifecycle

| Command | What it does |
|---|---|
| `docker compose up --build -d` | Build images (or rebuild if Dockerfile changed) and start all services detached |
| `docker compose up -d` | Start services without rebuilding (uses cached images) |
| `docker compose down` | Stop and remove containers; named volumes (Postgres data) survive |
| `docker compose down -v` | Stop and remove containers AND volumes — wipes the database |
| `docker compose build` | Rebuild images without starting |
| `docker compose ps` | List running containers and their status / healthcheck state |
| `docker compose logs -f backend` | Follow live logs for the backend container |
| `docker compose logs -f db` | Follow live logs for Postgres |

## Getting a shell inside a container

# Postgres

## C++ dev shell as user dev
docker compose exec backend bash
## Postgres container shell as root
docker compose exec db bash
## List databases from inside the db container
docker compose exec db psql -U atenciosamente -d atenciosamente_dev -c '\l'
## Open interactive psql session
docker compose exec db psql -U atenciosamente -d atenciosamente_dev

#  WSL2 / Docker permission helpers
## Activate docker group in current shell (no logout needed)
newgrp docker
## Add user to docker group permanently (takes effect on next login)
sudo usermod -aG docker $USER
## Fully restart WSL2 Ubuntu (run from Windows PowerShell, not WSL)
wsl --terminate Ubuntu


# Dev Container (VS Code)
Start containers: docker compose up -d
In VS Code: Ctrl+Shift+P → Dev Containers: Reopen in Container
In VS Code: Ctrl+Shift+P → Dev Containers: Rebuild Container      ← rebuilds image and restarts (use after changing devcontainer.json or Dockerfile)
In VS Code: Ctrl+Shift+P → Dev Containers: Rebuild and Reopen in Container  ← same but also re-attaches the window
Status bar shows [Dev Container: Atenciosamente Backend] when attached
Terminal inside VS Code is a shell in the backend container as user dev