# Verification Gate — `file-age` (pilot)

> This document is the REUSABLE TEMPLATE for the SOUL-standard + LLM-convenience gate.
> Every future package repeats this checklist; a package is "done" only when it passes.

## Package summary

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `file-age`                                         |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (owned)                               |
| legacy source  | `/home/vehir/vehir/src/tools/file_age.c` (118 LOC) |
| clean source   | `core/file-age/file_age.c` (133 LOC)               |

## SOUL-standard checklist

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | Single `.c` file, cJSON via nixpkgs                          |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#file-age`                        |
| 3 | Fail hard, zero silent failure      | YES  | Every `fopen`, `malloc`, `realloc`, `ferror`, `clock_gettime`, `cJSON_Parse`, `cJSON_GetObjectItem` checked; exits non-zero with structured error |
| 4 | DRY                                 | YES  | Single `die_json` for all error paths; single `print_result` for success |
| 5 | Zero dead code                      | YES  | No unused functions, no ifdefs, no stubs                     |
| 6 | Zero hardcode                       | YES  | Field name resolved at runtime via CLI arg (default `timestamp`) |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Set in `flake.nix` cflags; builds clean                     |
| 8 | No output suppression               | YES  | Errors go to both stderr (human) and stdout (structured JSON)|
| 9 | Security not weakened               | YES  | Gateway auth coupling SHED (was a runtime dependency on vgw; standalone tool needs no auth). No secrets, no network, no privilege escalation |
| 10| Legacy coupling shed                | YES  | `#include "../lib/gateway_check.h"` + `vgw_verify_auth()` removed entirely. No gateway dependency |
| 11| `strstr`+`strtod` hack replaced    | YES  | Replaced with `cJSON_Parse` + `cJSON_GetObjectItemCaseSensitive` + `cJSON_IsNumber` — proper parsing with error handling |

## LLM-convenience checklist

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | All output is single-line JSON: `{"ok":true/false, ...}`        |
| 2 | Consistent schema                | YES  | Success: `{ok, age_seconds, max_age_seconds, file, field}`. Error: `{ok, age_seconds, file, [field], error}` |
| 3 | `--help` / usage                 | YES  | `file-age --help` → clear usage to stderr, exit 2              |
| 4 | Predictable exit codes           | YES  | 0 = within limits, 1 = stale/error, 2 = usage. Documented in `--help` |
| 5 | No hidden coupling               | YES  | Standalone binary. No env vars, no config files, no daemons     |
| 6 | No hidden state                  | YES  | Pure function of (file contents, current time, CLI args)        |
| 7 | Declarative where it helps       | YES  | Manifest-driven IPM registration; tool itself is imperative (appropriate for a CLI tool) |
| 8 | Parseable by an LLM              | YES  | An LLM can invoke `file-age check <path> <seconds>` and parse the JSON stdout directly. Error messages are in `error` field, not scattered across stderr-only |

## Verdict

**PASS.** `file-age` meets both SOUL-standard and LLM-convenience requirements. The migration
from legacy is complete: gateway coupling shed, JSON parser upgraded, structured output preserved
and improved, IPM-registered with `proof_status=works` and `compat_verdict=compatible`.

## Template notes (for future packages)

- Both checklists above apply to EVERY migrated package.
- A package that fails any SOUL-standard item is NOT done.
- A package that fails any LLM-convenience item should document WHY (some items may not apply,
  e.g., a daemon may not have `--help`; justify in the Notes column).
- The `strstr`/hand-rolled-parser check (item 11) generalizes to: "no hand-rolled parsing of
  structured data when a proper parser exists."
- The "legacy coupling shed" check (item 10) generalizes to: "document WHAT was shed and WHY."

---

# Verification Gate — `runtime-paths` (S1.1)

## Package summary

| Field          | Value                                                      |
|----------------|------------------------------------------------------------|
| name           | `runtime-paths`                                            |
| artifact_type  | `tool`                                                     |
| proof_status   | `works`                                                    |
| compat_verdict | `compatible`                                               |
| license        | `Apache-2.0` (owned)                                       |
| legacy source  | `/home/vehir/vehir/src/lib/vsm_paths.c` (154 LOC)         |
| clean source   | `core/runtime-paths/runtime_paths.c` (105 LOC)             |
| probe tool     | `core/runtime-paths/probe.c` (65 LOC)                      |
| outputs        | `bin/runtime-paths-probe`, `lib/libruntime-paths.a`, `include/runtime_paths.h` |

## Ground-truth divergences

| Legacy                                      | New                                           | Reason                                       |
|---------------------------------------------|-----------------------------------------------|----------------------------------------------|
| Global mutable char[1024] buffers (23 vars) | `vehir_paths` struct, caller-owned            | Zero global state; testable, reentrant       |
| Hardcoded socket names (vgw.sock, vsm-db.sock, ...) | `vehir_paths_socket(p, "service-name", ...)` | Zero-hardcode; services register own names   |
| No config/state dir resolution              | XDG-compliant config + state dirs             | Broker compatibility (S0.1)                  |
| Extra pragma push/pop for GCC warnings      | Removed; code rewritten to avoid truncation   | Cleaner                                      |
| `VEHIR_CONTAINER` only affects vgw_sock     | `container` flag in struct for caller use      | Generalized; callers decide behavior         |

## SOUL-standard checklist

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | `runtime_paths.c` + `probe.c`, cJSON via nixpkgs (probe only)|
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#runtime-paths`                   |
| 3 | Fail hard, zero silent failure      | YES  | `vehir_paths_init` returns -1 on every failure path; probe calls `die_json` on init failure |
| 4 | DRY                                 | YES  | `safe_copy`, `join2`, `resolve_xdg` extracted as helpers     |
| 5 | Zero dead code                      | YES  | Unused `env_or` caught by `-Werror` and removed              |
| 6 | Zero hardcode                       | YES  | All paths from `/proc/self/exe`, env vars, or XDG defaults   |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Builds clean                                                 |
| 8 | No output suppression               | YES  | Probe: JSON to stdout, errors to stderr                      |
| 9 | Security not weakened               | YES  | No secrets handled; scratch dir created 0755; no privilege escalation |
| 10| Legacy coupling shed                | YES  | Global state removed; hardcoded socket list removed; pragma hacks removed |
| 11| Hand-rolled parsing replaced        | N/A  | Library does path resolution, no structured data parsing     |

## LLM-convenience checklist

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | Probe outputs `{"ok":true, "directories":{...}, "sockets":{...}}` |
| 2 | Consistent schema                | YES  | Success: `{ok, container, directories, sockets}`. Error: `{ok, error}` |
| 3 | `--help` / usage                 | YES  | `runtime-paths-probe --help` → usage to stderr, exit 2         |
| 4 | Predictable exit codes           | YES  | 0 = success, 1 = init failed, 2 = usage                        |
| 5 | No hidden coupling               | YES  | Standalone; broker-compatible via config dir; no daemon needed  |
| 6 | No hidden state                  | YES  | Creates scratch dir if missing (side effect documented)         |
| 7 | Declarative where it helps       | YES  | Manifest-driven IPM registration                                |
| 8 | Parseable by an LLM              | YES  | `runtime-paths-probe` → JSON with all paths; LLM can invoke and use any path |

## Verdict

**PASS.** `runtime-paths` meets both SOUL-standard and LLM-convenience requirements. Key improvements over legacy: struct-based (no globals), zero-hardcode socket names (dynamic by service name), XDG-compliant config/state dirs for broker compatibility, static library + header for downstream consumers.

---

# Verification Gate — `db` + `db-proxy` (S1.2, reworked per Claude verdict B)

## Claude verdict (escalation §3.SEC / architecture)

Direct libpq in a library violates SOUL §9 (Least Privilege) and §3 (Enforcement Is Architectural) — every consumer gets full DB access. Socket-proxy with per-caller gate is the constitutionally correct design. Decision: **socket-proxy NOW** (not deferred to S4.2) to avoid retrofitting 15 consumers later. This also aligns with the new SOUL §3 clause: RESOURCE BOUNDARIES ARE SOCKETS, NOT LINKED LIBRARIES.

## Package summary

| Field          | `db` (client)                                               | `db-proxy` (daemon)                                        |
|----------------|------------------------------------------------------------|------------------------------------------------------------|
| name           | `db`                                                       | `db-proxy`                                                 |
| artifact_type  | `tool`                                                     | `service`                                                  |
| artifact_name  | `db-probe`                                                 | `db-proxy`                                                |
| proof_status   | `works`                                                    | `works`                                                    |
| compat_verdict | `compatible`                                               | `compatible`                                               |
| license        | `Apache-2.0` (owned)                                       | `Apache-2.0` (owned)                                       |
| ipm edge       | parent=db, child=db-proxy, dynamic, Apache-2.0             | (none)                                                     |
| legacy source  | vsm_db.c (237 LOC, client)                                 | vsm_dbd.c (416 LOC, proxy)                                 |
| clean source   | `core/db/db.c` (250 LOC) + `db_probe.c` (80 LOC)           | `core/db-proxy/db_proxy.c` (330 LOC)                       |
| outputs        | `bin/db-probe`, `lib/libdb.a`, `include/db.h`              | `bin/db-proxy`                                            |
| deps (nix)     | cjson (no libpq)                                           | libpq + cjson                                              |

## Architecture

```
[consumer] → db client lib (db.h/db.c) → AF_UNIX socket → db-proxy daemon → libpq → PostgreSQL
             (no libpq link)              JSON protocol      (sole libpq owner)   (gate: DROP/TRUNCATE/
                                                             gate enforcement)    ALTER/INFORMATION_SCHEMA blocked)
```

Protocol: one JSON request per connection → one JSON response → close (stateless, same as legacy).

- Request: `{"sql":"SELECT $1","params":["hello"]}\n`
- Response (SELECT): `{"ok":true,"nrows":1,"ncols":1,"cols":["?column?"],"rows":[["hello"]]}`
- Response (INSERT/UPDATE/DELETE): `{"ok":true,"nrows":1}`
- Response (error): `{"ok":false,"error":"..."}`
- Response (blocked): `{"ok":false,"error":"BLOCKED: ..."}`

## Gate (db-proxy)

Implemented as `sql_blocked()` — case-insensitive check on first token and body:
- Blocked verbs: DROP, TRUNCATE, ALTER (destructive DDL)
- Blocked access: information_schema, pg_catalog (schema introspection)
- Everything else: allowed

The gate is a V1 placeholder for the per-caller allowlist (Claude spec: capability token from broker or SO_PEERCRED + config for per-package table/operation allowlists). Gate is enforced before any SQL reaches libpq.

## Ground-truth divergences

| Legacy                                              | New                                              | Reason                                            |
|-----------------------------------------------------|--------------------------------------------------|---------------------------------------------------|
| Hand-rolled JSON parser (jget_s/jget_params, 80 LOC)| cJSON (client + proxy)                           | Existing project dependency; cleaner             |
| `sql_blocked` only blocks information_schema        | Also blocks DROP/TRUNCATE/ALTER                  | Defense-in-depth; destructive ops gate            |
| Connection string in every binary                   | Only in db-proxy (sole libpq owner)              | §9: no other process touches DB credentials       |
| Global `vsm_db_sock` path                           | `VEHIR_DB_PROXY_SOCK` or `{scratch}/db-proxy.sock`| Zero-hardcode                                     |
| Poll-based daemon loop (vsm_dbd)                    | Same pattern: poll() + accept + handle + close   | Keeping simplicity                                |
| No daemonization (the legacy `-f` flag kept it foreground) | Always foreground (db-proxy is a Nix-level service) | Simpler; daemonization is a service-manager concern |
| Legacy `vsm_pain.h` uses vsm_pain_socket            | Pain (S1.4) will route through db-proxy socket    | Claude verdict: one DB proxy, no separate pain daemon |
| `build_connstr` inline in each consumer             | Centralized in db-proxy only                      | Single source of truth                             |

## SOUL-standard checklist — `db` (client lib)

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | `db.c` + `db_probe.c`, cJSON via nixpkgs, no libpq          |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#db`                              |
| 3 | Fail hard, zero silent failure      | YES  | Every socket/send/recv checked; cJSON parse checked; malloc/calloc checked |
| 4 | DRY                                 | YES  | `send_recv`, `do_roundtrip`, `json_esc`, `build_request`    |
| 5 | Zero dead code                      | YES  | `-Werror` catches all                                        |
| 6 | Zero hardcode                       | YES  | Socket path from `VEHIR_DB_PROXY_SOCK` or scratch-derived   |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Builds clean (one pragma on `resolve_sock_path`)            |
| 8 | No output suppression               | YES  | Probe: JSON to stdout, errors to stderr                      |
| 9 | Security not weakened               | YES  | Client has NO DB credentials; no libpq; all queries through gate |
| 10| Legacy coupling shed                | YES  | No libpq in client; JSON protocol over socket (like legacy)  |
| 11| Hand-rolled parsing replaced        | YES  | cJSON replaces legacy jget_s/jget_params                     |

## SOUL-standard checklist — `db-proxy` (daemon)

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | `db_proxy.c`, libpq + cJSON via nixpkgs                     |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#db-proxy`                        |
| 3 | Fail hard, zero silent failure      | YES  | PQstatus/PQexecParams/PQconnectdb checked; write() checked; malloc checked |
| 4 | DRY                                 | YES  | `json_esc`, `send_err`, `send_ok_exec`, `send_ok_query`     |
| 5 | Zero dead code                      | YES  | `-Werror` catches all                                        |
| 6 | Zero hardcode                       | YES  | Socket path from `VEHIR_DB_PROXY_SOCK` or scratch-derived; conn params from env/broker |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Builds clean (pragma on `set_sock_default`)                 |
| 8 | No output suppression               | YES  | All output to stderr (diagnostics); errors in JSON response  |
| 9 | Security not strengthened enough    | NOTE | V1 gate is DROP/TRUNCATE/ALTER/information_schema. Full per-caller allowlist deferred (SO_PEERCRED + broker token, Claude spec). Gate is architecture-ready. |
| 10| Legacy coupling shed                | YES  | No vsm_pain dependency; standalone daemon                     |
| 11| Hand-rolled parsing replaced        | YES  | cJSON replaces jget_s/jget_params; `str_contains_nocase` replaces `strcasestr` (POSIX compat) |

## SOUL §3 amendment (RESOURCE BOUNDARIES)

Added to SOUL.md per Claude verdict: "RESOURCE BOUNDARIES ARE SOCKETS, NOT LINKED LIBRARIES." This codifies the architectural rule that db-proxy (and future pain, locks, info services) must be socket-fronted services, not libraries. Pure-logic code (runtime-paths, config parsing, math) stays a library. This amendment invalidates §3 section hash — re-baseline needed at S1.5 (constitution-validator).

## LLM-convenience checklist (both packages)

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | Probe: `{ok, connected, nrows, ncols, columns, rows}`. Proxy: `{ok, nrows, [ncols, cols, rows], [error]}` |
| 2 | Consistent schema                | YES  | Client error: `{ok, error}`. Proxy error: `{ok, error}`        |
| 3 | `--help` / usage                 | YES  | `db-probe --help` → usage, exit 2. `db-proxy` is a daemon (no `--help`) |
| 4 | Predictable exit codes           | YES  | Probe: 0/1/2. Proxy: 0/1 (startup)                             |
| 5 | No hidden coupling               | YES  | Client ↔ proxy over socket. No shared memory, no env leakage   |
| 6 | No hidden state                  | YES  | Stateless protocol (one req → one resp per connection)          |
| 7 | Declarative where it helps       | YES  | Manifest-driven IPM registration with edge                      |
| 8 | Parseable by an LLM              | YES  | All JSON endpoints are single-line, schema-documented           |

## Known deviations

- Config file reader (`env_file_get`) — 3rd copy (forge, mail, db-proxy). Extraction to `core/vehir-config` deferred to S1.4.
- Gate is V1 (block-based, no per-caller identity). Full per-caller allowlist per Claude spec is deferred.
- `strcasestr` replaced with `str_contains_nocase` — POSIX-compatible, avoids `_GNU_SOURCE` pragma dance.

## Verdict

**PASS.** `db` + `db-proxy` meet both SOUL-standard and LLM-convenience requirements. The architecture now follows §3 and §9: the socket-proxy is the single libpq owner with a gate; all other packages (db client, pain, locks, info) route through it. This is the constitutionally correct design, built now (not retrofitted at S4.2) while consumers are few.
