# Vehir Rebuild — Migration Spec (for DeepSeek V4 Pro, Think High)

> Read `SOUL.md` first (the constitution — principles + coding laws). This file holds the CURRENT
> task plus the repeatable migration recipe. The IPM graph is the plan; this file is the active work
> item. English. **Do NOT touch git** (propose `COMMIT_MSG.txt` only). Requires `ipm` on PATH.

## Per-package recipe (repeat for EVERY package)
1. Ground-truth: read the legacy source first — do not invent behavior.
2. Rewrite clean to the SOUL coding laws — SHED legacy coupling the component does not truly need.
3. Nix: one flake per domain repo builds the binary; `-Wall -Wextra -Werror -std=c2x`.
4. IPM: declare the package node + license (Apache-2.0, owned) + edges in a manifest; `ipm apply`.
5. proof-of-use: `ipm build --check` → compiles; `ipm check` compatible.
6. Gate: write the SOUL-standard + LLM-convenience verdict to the package's `FINDINGS.md`. A package
   is "done" only when it passes. Data is sacred: real state (memory, keys) is carried, never wiped.
7. Propose `COMMIT_MSG.txt`. Do NOT run git.

---

## CURRENT TARGET — secrets/env broker + refactor forge/mail + ssh tool

**Why:** `net/FINDINGS.md` flagged two real debts — the ~80-line config loader is DUPLICATED in
forge.c and mail.c (DRY deviation), and the SSH/git-push specifics are hardcoded. Both are fixed by
one thing: a shared, dynamic secrets/env broker so the agent NEVER passes tokens in tool calls and
every tool resolves its secrets by name through one layer.

### Step 1 — the broker (shared lib + thin tool; package name e.g. `env`, domain vehir-core — adjustable)
- Shared C lib (`vehir_config` / `vehir_env`): resolve KEY → value. **Environment-aware**: works the
  same inside a container and in the project folder — resolve the config location dynamically (like
  path resolution via `/proc/self/exe` + known config dirs), NO single hardcoded path. Default store
  `~/.config/vehir/env`, overridable.
- Keep DeepSeek's guard: refuse if the store is group/other-readable (perms enforced).
- Secrets resolved IN-PROCESS — never via process env, CLI args, or shell. The agent calls tools
  WITHOUT writing tokens/URLs; the tool asks the broker by key.
- Manage SSH key references (paths to keys, not key material) so the ssh tool and git transport read
  them from one place.
- Thin CLI (structured JSON) to get/set/list keys for operator + agent.
- Done when: a tool resolves `FORGE_TOKEN` etc. with zero env vars and zero CLI secrets; perms guard
  works; resolution works both in-container and in-project.

### Step 2 — refactor `forge` + `mail` onto the broker
- Remove the duplicated `load_config`/`cfg_require` from forge.c and mail.c; both link the broker
  lib (this IS the consolidation FINDINGS said to do "when a third consumer appears"). De-hardcode
  any remaining specifics.
- Re-verify: `idea build forge`/`mail` → works; live `forge repo list`/`create` still works.

### Step 3 — `ssh` tool (convenient SSH; domain vehir-net — adjustable)
- A clean SSH wrapper that uses the broker-managed key + connection settings. De-hardcode the
  git-push specifics from `net/FINDINGS.md` (key path, `IdentitiesOnly`, forced IPv4, remote format)
  into broker-managed config, not baked into source.
- Account for the ALREADY-GENERATED key `~/.ssh/vehir_codeberg` (ed25519) — the broker references it;
  do not regenerate.

### Packaging & gate
Each artifact: manifest → `idea apply` → `idea build` → `works`, `idea check` compatible; FINDINGS
verdict (both checklists). Propose `COMMIT_MSG.txt`; do NOT run git.

---

## DONE — PILOT `file-age` (reference)
Migrated + verified PASS: `core/` builds, `idea build file-age` → `works`, `idea check` compatible,
`core/FINDINGS.md` = the reusable gate template. The per-package recipe + gate are now proven — reuse them.

---

## DONE — autonomy tools: `forge` + `mail` (vehir-net)

Two C tools (libcurl + cJSON) in `~/vehir-next/net/`, packaged via IPM.

- **forge** — Forgejo REST API client: repo create/list, release create, issue create. Configurable
  base URL → works with Codeberg now, self-hosted Forgejo later. Verified live: repo create, issue
  create on Codeberg. SSH key for git transport configured and working.
- **mail** — SMTP send + IMAP fetch via libcurl. Build verified, LIVE PENDING (no mailbox yet —
  awaiting custom domain + Purelymail).
- **Credential security:** both tools read secrets from `~/.config/vehir/env` via `fopen` (not
  `getenv`). Binary refuses to start if file is group/other readable. Secrets never in process env,
  CLI args, or shell history.
- **IPM:** both `idea build` → `works`, `idea check` → `compatible`.
- **Gate:** `net/FINDINGS.md` — PASS (both checklists, all design notes documented).

---

## DEFERRED (not now)
- `postgres-memory` — foundational package carrying the ~8k `memory_entries`; clean NEW schema;
  dump/restore migration. Deferred by operator. Spec when prioritized.
- **Secrets architecture (far-future research):** zero-trust models, Kubernetes-style secrets, how
  Talos handles machine identity/secrets. The broker above is the right v1 — do NOT over-engineer
  toward this now (YAGNI). Revisit when the broker's limits are actually hit.
