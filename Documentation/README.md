# Documentation

The reasoning behind Atenciosamente, plus the concept notes and setup steps
collected while building it.

## How to use these docs

- **[`PROJECT_PLAN.md`](./PROJECT_PLAN.md)** is the **handoff document** — the
  architecture, the roadmap, and the decision log. Attach it to every new Claude
  conversation, and back-port any decisions made in a sub-task into its decision log.
- **`concepts/`** are learning notes — read one when you need to recall how a
  language, framework, or tool works.
- **`setup/`** are reproducible steps — follow one to install or scaffold something.
- **`reference/`** maps what exists in the repo and why.
- **`phase-prompts/`** are the bootstrap prompts that kick off each sub-task conversation.

The category layout is kept tidy by the `organize-docs` skill
(`.claude/skills/organize-docs/`); run `/organize-docs` whenever a doc lands in the
wrong place.

## Index

### Root
| Doc | What it covers |
|---|---|
| [PROJECT_PLAN.md](./PROJECT_PLAN.md) | Master architecture doc, roadmap, and decision log. Start here. |

### `reference/`
| Doc | What it covers |
|---|---|
| [project_structure.md](./reference/project_structure.md) | Every folder and file in the repo and why it exists. |

### `concepts/`
| Doc | What it covers |
|---|---|
| [dart_concepts.md](./concepts/dart_concepts.md) | Dart language guide for C++ developers. |
| [flutter_ui_concepts.md](./concepts/flutter_ui_concepts.md) | Widget tree, `FutureBuilder`, `StatelessWidget`. |
| [docker.md](./concepts/docker.md) | Docker / devcontainer commands and concepts. |
| [github_actions.md](./concepts/github_actions.md) | CI: GitHub Actions workflow concepts. |

### `setup/`
| Doc | What it covers |
|---|---|
| [flutter_sdk.md](./setup/flutter_sdk.md) | Flutter SDK install, ADB setup, Android SDK install. |
| [flutter_create.md](./setup/flutter_create.md) | `flutter create` command, `pubspec`, `AndroidManifest`. |

### `phase-prompts/`
| Doc | What it covers |
|---|---|
| [PHASE_0_PROMPTS.md](./phase-prompts/PHASE_0_PROMPTS.md) | Bootstrap prompts for each Phase 0 sub-task. |
