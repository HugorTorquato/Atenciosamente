---
name: organize-docs
description: >-
  Tidy the Documentation/ folder of the Atenciosamente repo into its canonical
  category layout (reference/, concepts/, setup/, phase-prompts/), keeping
  PROJECT_PLAN.md at the root. Use when docs have drifted, a new doc landed in
  the wrong place, or the structure needs re-applying. Documentation-only —
  never touches code, build files, or CI.
---

# Organize the Documentation folder

You keep `Documentation/` tidy. This is a **docs-only** task: never modify
anything under `backend/`, `mobile/`, `.github/`, or build/compose files.

## Canonical layout

```
Documentation/
├── README.md                 ← index of every doc
├── PROJECT_PLAN.md           ← stays at root (handoff doc / decision log)
├── reference/                ← maps of "what exists and where"
│   └── project_structure.md
├── concepts/                 ← language / framework / tooling learning notes
│   ├── dart_concepts.md
│   ├── flutter_ui_concepts.md
│   ├── docker.md
│   └── github_actions.md
├── setup/                    ← reproducible install / scaffold steps
│   ├── flutter_sdk.md
│   └── flutter_create.md
└── phase-prompts/            ← bootstrap prompts, one file per phase
    └── PHASE_0_PROMPTS.md
```

## Where each kind of doc goes

- "How a language/framework/tool works" (learning note) → `concepts/`
- "How to install or scaffold something, step by step" → `setup/`
- "What files/folders exist and why" (a map) → `reference/`
- "Prompt to bootstrap a sub-task conversation" OR "the plan for a phase"
  (e.g. `PHASE_1_PERSISTENCE.md`) → `phase-prompts/`
- The master plan + decision log → `PROJECT_PLAN.md` at the root (never move it).

## Procedure

1. List `Documentation/` recursively and compare against the canonical layout.
2. For every misplaced file, `git mv` it to the right bucket (preserve history).
3. Delete stale duplicates — e.g. any dated `*_PROJECT_PLAN.md` that duplicates
   `PROJECT_PLAN.md` (diff them first to confirm they're equivalent).
4. Remove now-empty legacy folders (`CI/`, `contexts/`, `Dependencies/`,
   `Phase_Prompts/`).
5. Rewrite `Documentation/README.md` as an index: one line per doc + a short
   "how to use these docs" note.
6. Update the "Documentation —" section of `reference/project_structure.md` to
   match the layout.
7. Grep the repo for links to old doc paths (root `README.md`,
   `PHASE_0_PROMPTS.md`, etc.) and fix any stragglers.
8. Finish with a **single commit**, message style `Scope (Tag): summary`
   (e.g. `Docs (Reorg): tidy Documentation into category folders`). No body,
   no trailers.

If the docs are already in canonical form, say so and make no changes.
