# Vehir Rebuild — Long Roadmap

> This is an in-progress clean rebuild of Vehir as idea-packages managed by IPM. The IPM package
> graph is the canonical plan; this file is the migration roadmap. Consistent with MIGRATION.md.

---

## 0. Roles & Protocol (applies to EVERY step)

**Roles:**
- **Builder** — writes code, manifests, COMMIT_MSG.txt. Does not touch git.
- **Verifier** — (a) collects ground-truth before each step (legacy sources, DB schema, current
  state); (b) checks results (runs `idea build`/`idea check`, runs the binary, reads FINDINGS) and
  delivers a pass/fail verdict.
- **Operator** (human) — git commit/push, human-only steps (accounts), relays to architect/reviewer
  on escalation.
- **Architect/Reviewer** — deliberation and escalation only. Does not touch files.

**Dynamicity protocol (this IS "something is wrong / changed"):**
1. Ground-truth precedes spec. Spec gives INTENT + done-criteria, not exact strings. The verifier
   collects the real legacy source; if it differs from the description, the builder adapts to the
   done-criterion and writes the divergence in FINDINGS.
2. Divergence rules:
   - Legacy component missing/different → build from done-criterion (intent), note in FINDINGS.
   - Build fails → fix per coding laws; package not in nixpkgs → overlay, not workaround.
   - Dependent package not ready → declare interface stub OR reorder; do not block silently.
   - `idea check` = incompatible/unknown → resolve license/edge; `--force` only with justification
     in FINDINGS.
   - ARCHITECTURAL assumption changes (spec is wrong at design level) → escalate to
     architect/reviewer.
3. End of each step: `idea apply` → `idea build <pkg>` = works → `idea check` = compatible →
   FINDINGS (both checklists: SOUL-standard + LLM-convenience). Verifier checks and delivers
   PASS/FAIL + FINDINGS.
4. Data is sacred. `memory_entries` (~8k), keys, `death_ledger` — carry via dump/restore, NEVER
   bring up empty on top of existing state.
5. Secrets — through the broker (S0.1), not env/CLI/hardcoded.
6. Escalate to architect/reviewer ONLY at: architectural divergence · §3.SEC/security question ·
   cross-package design decision · repeated failure (>2 attempts) · items marked ⚠️ below.
   Otherwise — proceed autonomously.

**Invariants (from SOUL v11):** C-only (GPU exception for MSA), Nix-only build, fail-hard,
zero-hardcode, declarative>imperative, naming-law, least-privilege, security only ever strengthened.

---

## Phase 0 — Foundation

### S0.1 — secrets/env broker + refactor forge/mail + ssh (ALREADY in MIGRATION.md)
Do first: everything else depends on it (configs, tokens, keys). Done — see MIGRATION.md.

### S0.2 — IPM ROUND 4: compat fail-closed + real rules (ALREADY in ipm/SPEC.md)
Without checker teeth, packages with copyleft dependencies cannot be safely carried. Done — see
ipm/SPEC.md R4.1/R4.2.

---

## Phase 1 — Runtime substrate (repo vehir-core)

### S1.1 — runtime-paths (lib)
Dynamic path resolution (root, cache, sockets) via `/proc/self/exe` + config, like legacy
`vsm_paths.c`, but zero-hardcode and broker-compatible. Ground-truth: `src/lib/vsm_paths.c`.
Done: tool probe prints resolved paths as JSON, both in-container and in-project.

### S1.2 — db (lib + optional proxy daemon)
Postgres access. Ground-truth: `src/services/vsm_dbd.c` (the single libpq consumer, socket proxy).
Decision ⚠️: keep "one process with libpq" (proxy daemon) OR let packages link libpq directly (as
IPM does)? Default: lib on libpq + connection config through broker; proxy daemon only if isolation
truly requires it (justify in FINDINGS). Bind parameters mandatory (no interpolation).
Done: lib performs parameterized SELECT/INSERT, fail-hard.

### S1.3 — postgres-memory (DATA — careful)
Clean NEW memory schema: `memory_entries` (halfvec embeddings, multilingual FTS), record types,
importance, layers. Ground-truth: `scripts/migrations/000_initial_schema.sql` + verifier atlas §1.
Carry ~8k records: pg_dump needed tables from legacy → transform to new schema → restore. Never
empty the database. Depends: S1.2. Done: new schema applied via `idea init`-like path; control N
records carried and found by search; before/after counts match. ⚠️: final table list to carry
(`memory_entries` — definitely; `code_index`/`death_ledger`/`skills_catalog` — confirm) is decided
when reached.

### S1.4 — pain (lib + signal)
`pain_registry` + `compute_total_pain` + mapping to cognitive mode (SOUL §5). Ground-truth:
`src/services/vsm_pain.c`, `src/tools/pain_check.c`. Depends: S1.2. Done: write/resolve signal,
read total pain, threshold→mode.

### S1.5 — constitution-validator (rebuild soul_hash, fix fragility)
Section hashing of SOUL.md + drift detection. Ground-truth: `src/tools/soul_hash.c`. Key change
(dynamicity): (a) make "referenced item exists" check NON-fatal for the constitution itself
(service "not active right now" ≠ constitution drift) — this was a bug; (b) validate only section
hash drift + optionally soft-warn about references; (c) re-baseline v11 hashes. Depends: S1.2.
Done: on v11 `SOUL --verify` = ok after sync; temporarily inactive daemon does NOT fail the
constitution.

---

## Phase 2 — Memory & Analysis (repo vehir-mind)

### S2.1 — msa-engine (daemon, Python/GPU — documented exception)
MSA-4B as persistent daemon (loads model once). Ground-truth: `scripts/vsm_msa.py` vs
`MSA/src/msa_service.py` — first establish the canon (which actually loads the model, what it
imports). Depends: S1.3, GPU (6GB, shared with Ollama → lazy-load/idle-unload).
Done: socket daemon, single-instance, 2-stage rerank in <30s (warm). Thin clients
(msa-search/rerank) — no model reload. Dynamic: if the canon daemon is not what the spec says —
record in FINDINGS and build from the real one.

### S2.2 — lens (analysis pipeline)
Layers: RAW parser (capture everything) → mechanical rules (risk_score without LLM) → FTS+embedding
→ MSA rerank (S2.1) → machine-readable report. Ground-truth: `vextract.c`, `code_index`
(coverage/freshness), `search_v3.c`, `smart-search`, `plain-read`, `tools/sg`. This is
consolidation, not write-from-scratch. Depends: S2.1, S1.3. Done: `review <target>` outputs JSON
findings (spaghetti/duplicates/dead code/risk); on a known bad file finds ≥1 real problem.

### S2.3 — search (canonical)
Single tool: `--raw` (source) + `smart` (FTS+pgvector) + `--deep` (→ MSA rerank S2.1). Consolidate
duplicate search paths into one. Depends: S2.2/S2.1. Done: smart returns JSON top-N; raw returns
full file; deep returns reranked top-K.

---

### S3.1 — locks + activity + info (small infra packages on S1.2)
Transactional locks (reason/ttl/source mandatory), event feed (event-driven), info/hints from
table. Ground-truth: `vsm_locks.c` / `vsm_activity.c` / `vsm_info.c`.

### S3.2 — enforcement-checks (static checks)
23 legacy checks → clean C modules; remove Python-dependent checks or rewrite. Ground-truth:
`src/services/enforcer/checks/`. ⚠️: sniper-vs-bwrap-vs-positive-enforcement direction — NOT
DECIDED; do not touch enforcement architecture without architect/reviewer.

### S3.3 — positive-enforcement (on S2.2 lens)
Mine tool-call logs → inefficiency patterns → blacklist/improvement. Depends: lens.

### S3.4 — task-coordination
ONE clean task/todo/delegation model (legacy has two — `v_core_*` and `delegated`; consolidate).
Ground-truth: schema + `vgw_scheduler.c`.

---

## Phase 4 — Runtime life (LOOP, after Phase 3)

### S4.1 — service-lifecycle (rebuild vsm-init, event-driven, declarative manifests in DB)
Ground-truth: `vsm_init.c`, `vsm/services/`. ⚠️ (major design).

### S4.2 — isolation-physics (landlock/bwrap/container — "physics")
⚠️ + §3.SEC: security only ever strengthened; sniper-vs-bwrap not decided.

### S4.3 — swarm (leader/worker on C+Postgres: shared board, heartbeat)
### S4.4 — reliability-cage (adversarial voting, Finch-Zk, SLM filter, logprobs→pain, constrained decoding, mathematical behavior verification)

---

## Phase 5 — Distant (names only)

`rsi-safe-modification`, `blackice`, `benchmark`, `end-user-distribution`, `free-time-engine`. Not
detailed until Phase 4.

---

## Escalation Criteria

1. **S1.3** — final table list to carry + schema transformation (data).
2. **S1.2** — proxy daemon vs direct libpq (if isolation is contested).
3. **S3.2 / S4.2** — anything on enforcement / isolation / sniper-vs-bwrap / §3.SEC (NOT DECIDED).
4. **S4.1, S4.3, S4.4** — major architectural designs.
5. Any divergence at "spec is architecturally wrong" level or repeated failure >2 times.
6. Final package list for the first public `vehir-*` repo release.

Otherwise — autonomous: S0.1 → S0.2 → S1.1 → S1.2 → S1.4 → S1.5 → S2.1 → S2.2 → S2.3 → S3.1.
That is ~10 packages without architect/reviewer. On each: ground-truth → build → `idea
apply`/`build`/`check` → FINDINGS → verifier verdict → COMMIT_MSG → operator commits.
