<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->

# Tvheadend Vue web UI

A Vue 3 single-page admin interface for Tvheadend, served at `/vue/`
alongside the ExtJS UI. It talks to the server over the **existing**
JSON REST API and the Comet push channel — there are no Vue-specific
server endpoints or protocols.

This document orients a contributor working on the UI. It describes
how the app is structured and built today; for user-facing feature
docs see the in-app Help and the project documentation site.

## How it fits the server

- The C side that mounts the app is `src/webui/vue.c` (registered from
  `webui_init()` in `src/webui/webui.c`). It serves the SPA shell at
  `/vue` and its built assets under `/vue/static`, both gated on
  `ACCESS_WEB_INTERFACE`.
- The compiled app lives in `dist/` and is served from disk. When the
  UI is not built, `/vue` serves a small fallback page instead.
- **Data flow.** Everything the UI shows or changes goes through the
  same API the ExtJS UI uses:
  - **REST** — `src/api/client.ts` wraps `POST /api/<endpoint>`
    (form-encoded in, JSON out, cookies/auth included).
  - **Comet** — `src/api/comet.ts` is the server→browser push channel
    (long-poll / WebSocket). Stores subscribe to notification classes
    (`accessUpdate`, `channel`, `dvrentry`, `logmessage`, …) for live
    updates.
- **idnode metadata drives the UI.** Tvheadend describes every
  configurable class via idnode metadata (`api/idnode/class`,
  `api/idnode/load?meta=1`). The grids, the editor form, column sets,
  enum options, and permission gating are all generated from that
  metadata — so most new server-side fields appear in the UI with no
  client change.

## Tech stack

- **Vue 3** (Composition API, `<script setup>`) + **TypeScript** (strict)
- **Vite** build, **Vitest** tests (happy-dom)
- **Pinia** state, **Vue Router** routing
- **PrimeVue 4** + `@primeuix/themes` components, **Lucide** icons
- **Chart.js** (bandwidth charts), **Fuse.js** (command-palette search)

All runtime dependencies are MIT/ISC; build-only dependencies are
MIT/Apache-2.0. `package-lock.json` is committed for reproducible
builds.

## Project layout (`src/`)

| Dir | What's in it |
|---|---|
| `api/` | REST client (`client.ts`), Comet client (`comet.ts`), idnode CRUD helpers |
| `stores/` | Pinia stores — `access`, `comet`, `log`, `capabilities`, `dvrConfig`, `dvrEntries`, `idnodeClass`, `wizard`, … |
| `composables/` | Reusable logic — EPG view state, grid layout/persistence, inline cell editing, stale-data recovery, … |
| `components/` | Shared UI — the grids (`IdnodeGrid`, `DataGrid`), the drawer editor (`IdnodeEditor`), metadata-driven field renderers (`idnode-fields/`), `ActionMenu`, `NavRail`, `ChannelLogo`, … |
| `views/` | Route components for each section (EPG, DVR, Configuration, Status, Home, setup wizard) |
| `layouts/` | App shell + the master-detail / page-tab section layouts |
| `router/` | Route table + `beforeEach` guards (permission gating, setup-wizard redirect) |
| `commands/` | Command-palette (⌘K) sources and action handlers |
| `styles/` | Design tokens, base styles, PrimeVue overrides |
| `types/` | Shared TypeScript types |
| `utils/` | Small helpers (formatting, URLs, markdown, …) |

Bootstrap entry points: `index.html` → `src/main.ts` → `src/App.vue`.

## Key concepts

- **Grids.** `DataGrid` is the presentation layer (sticky header,
  phone-card layout, server-side sort/filter/paginate, column
  show/hide/reorder/width persisted per grid). `IdnodeGrid` wraps it
  with idnode-class awareness (metadata-driven columns, inline cell
  editing, bulk actions). Status grids use `StatusGrid`.
- **Editor.** `IdnodeEditor` is the drawer form. Field widgets in
  `components/idnode-fields/` are chosen from each property's
  metadata type (string / int / bool / enum / multi-enum / time / …).
  Multi-row edit applies ticked fields across a selection in one POST.
- **Live updates.** Views stay current via Comet; the
  `useStaleDataRecovery` composable refetches after a long disconnect.
- **i18n.** Server-defined strings (column captions, field labels,
  enum options) localise automatically via the server metadata.
  UI-chrome strings use a `t(…)` composable backed by the same
  `/locale.js` catalog the ExtJS UI uses, so shared wording is
  translated once.
- **Themes.** Three themes (Blue / Grey / Access) are server-driven
  via the access object and applied through CSS custom properties.
- **Responsive.** Desktop tables, tablet horizontal scroll, phone
  card lists; drawers go full-width on phone.

## Common tasks

- **Add a configuration grid.** Create a view under
  `views/configuration/` that mounts `<IdnodeGrid>` with the idnode
  `class` and its `api/<x>/grid` endpoint; add a route under the
  Configuration tree in `router/index.ts` plus a nav / page-tab
  entry. Columns, inline editing, the editor form, and permission
  gating all come from the class metadata — you typically only pick
  the default-visible columns and any custom toolbar actions.
- **Add a field widget for a property type.** Field components live
  in `components/idnode-fields/` (`IdnodeFieldString`, `…Enum`,
  `…Bool`, `…Time`, `…Perm`, …). The metadata `type` is mapped to a
  component in `idnode-fields/rendererDispatch.ts` — add a case there
  and a matching `IdnodeFieldXxx.vue`. Validation lives in
  `idnode-fields/validators.ts`; server-resolved ("deferred") enum
  lists in `idnode-fields/deferredEnum.ts`.
- **Add a command-palette entry.** Command sources live in
  `commands/` (e.g. `channelSource.ts` exposes an
  `ensure…Loaded(deps)` that builds the command objects;
  `actionHandlers.ts` holds the side-effecting handlers). Add or
  extend a source and surface it from `CommandPalette.vue`.
- **React to a server change live.** Subscribe a Pinia store to the
  relevant Comet class (`cometClient.on('<class>', …)`) and use
  `useStaleDataRecovery` so the view also refetches after a long
  disconnect. Grid-backed views inherit this through `IdnodeGrid`.

## Conventions

- **Styling** — use the `--tvh-*` design tokens from
  `styles/tokens.css` (colours, spacing, radii, fonts); don't
  hardcode values. Component styles are `scoped`; reach a child
  component's internals with `:deep(...)`. Scoped styles do **not**
  apply to Teleported or slotted content — style those from a
  non-scoped block or via tokens.
- **i18n** — wrap every user-facing string in `t('…')` (from
  `composables/useI18n`). Reuse the wording the ExtJS UI already uses
  where it matches, so the string is already translated.
- **Permissions** — gate admin-only UI on `access.has('admin')` (or
  `'dvr'`) and set `meta.permission` on routes; the router guard
  enforces it. Anonymous / limited users must not trigger admin-only
  API calls on load.
- **Store lifecycle** — `access`, `comet`, `log`, and `capabilities`
  are instantiated eagerly during bootstrap (`main.ts`) so their
  Comet listeners are in place before the first message arrives;
  grid / data stores are created lazily by the views that use them.

## Known constraints

- **PrimeVue Drawer + `position: fixed`.** The Drawer applies a
  `transform`, which makes `position: fixed` descendants clip to the
  drawer. Popovers that must escape it (e.g. the overflow menu in
  `ActionMenu`) are `Teleport`ed to `<body>` and positioned in
  viewport coordinates.
- **VirtualScroller containing block.** Content that must break out
  of a virtualised list (e.g. the expand-in-place cell editor) has to
  escape the scroller's containing block — again via Teleport.
- **Phone mode.** The breakpoint is ~768 px (`max-width: 767px`).
  Below it, grids switch to a card layout, drawers go full-width, and
  hover-only affordances are hidden — check both widths when touching
  grid / drawer / toolbar UI.
- **In-browser playback.** The player uses a native `<video>`
  element, which covers Chrome / Firefox / Edge but not Safari / iOS
  (continuous MPEG-TS there needs Media Source Extensions). The
  external-player path (an `.m3u` via the ticket route) works
  everywhere.

## Build & develop

The production build is configure-gated on the C side (it mirrors the
optional-feature pattern of e.g. `--enable-libav`):

- `--enable-vue_build` (**default `auto`**) — build the UI locally with
  Vite. Requires `node` + `npm` on `PATH`.
- `--enable-vue_cache` / `--disable-vue_build` — fetch a prebuilt
  bundle instead of building locally (for packagers without a Node
  toolchain).
- With both disabled, the daemon still builds and `/vue` serves the
  fallback page.

Working on the UI directly (from this directory):

```bash
npm install
npm run dev          # Vite dev server with HMR
npm run build        # type-check (vue-tsc) + production build → dist/
npm run test         # Vitest (one-shot);  npm run test:watch for HMR
npm run type-check   # vue-tsc --noEmit
npm run lint         # ESLint + Stylelint + SPDX-header check
npm run format       # Prettier
```

The full daemon build (`make` at the repo root) invokes the Vite
build when `vue_build` is enabled and ships the result in `dist/`.

## Quality gates

TypeScript strict mode, ESLint + Prettier, Stylelint, and an
SPDX-header check all run clean. The Vitest suite covers stores,
composables, and components. There is no C-side unit-test framework;
server behaviour is validated by building cleanly (`-Werror`) and
exercising features at runtime.
