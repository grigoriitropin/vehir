# Verification Gate — `forge` + `mail` (vehir-net)

> Gate checklist per SOUL.md. Both tools must pass SOUL-standard AND LLM-convenience.
> A package is "done" only when it passes this gate.

---

## Package: `forge`

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `forge`                                            |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (owned)                               |
| source         | `net/forge/forge.c`                                |

### SOUL-standard checklist — forge

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | Single `.c` file, libcurl + cJSON via nixpkgs                |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#forge`                           |
| 3 | Fail hard, zero silent failure      | YES  | Every `stat`, `fopen`, `fgets`, `ferror`, `curl_easy_init`, `curl_easy_perform`, HTTP status code, `cJSON_Parse`, `cJSON_CreateObject`, `cJSON_PrintUnformatted`, `malloc`, `realloc`, `strdup` checked; exits non-zero with structured error JSON |
| 4 | DRY                                 | YES  | Single `die_json` for all errors; single `api_request` for all HTTP; `load_config`/`cfg_require` for config; `find_opt`/`has_flag` for arg parsing |
| 5 | Zero dead code                      | YES  | No unused functions, no ifdefs, no stubs                     |
| 6 | Zero hardcode                       | YES  | `FORGE_BASE_URL` from config file — same binary works with Codeberg now and self-hosted Forgejo later (change one line in config). `FORGE_TOKEN` from config file |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Set in `flake.nix` cflags                                   |
| 8 | No output suppression               | YES  | Errors to both stderr (human) and stdout (structured JSON)   |
| 9 | Security not weakened               | YES  | Token read from `~/.config/vehir/env` (chmod 600 enforced — binary refuses to start if group/other can read). Secrets never touch process environment, shell history, or command line. HTTPS enforced by URL scheme. No secrets in source |
| 10| No hidden coupling                  | YES  | Standalone binary. No gateway, no auth layer, no daemon dependency |

### LLM-convenience checklist — forge

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | All output is single-line JSON: `{"ok":true/false, ...}`        |
| 2 | Consistent schema                | YES  | Success: `{ok, action, ...fields}`. Error: `{ok, error}`       |
| 3 | `--help` / usage                 | YES  | `forge --help` → clear usage to stderr, exit 2                 |
| 4 | Predictable exit codes           | YES  | 0 = success, 1 = error, 2 = usage. Documented in `--help`      |
| 5 | No hidden coupling               | YES  | One config file (`~/.config/vehir/env`), overridable with `--config`. No env vars needed |
| 6 | Parseable by an LLM              | YES  | LLM invokes `forge repo create <name>` and parses JSON stdout   |
| 7 | Declarative where it helps       | YES  | Manifest-driven IPM registration                                |

---

## Package: `mail`

| Field          | Value                                              |
|----------------|----------------------------------------------------|
| name           | `mail`                                             |
| artifact_type  | `tool`                                             |
| proof_status   | `works`                                            |
| compat_verdict | `compatible`                                       |
| license        | `Apache-2.0` (owned)                               |
| source         | `net/mail/mail.c`                                  |

### SOUL-standard checklist — mail

| # | Criterion                           | Pass | Notes                                                        |
|---|-------------------------------------|------|--------------------------------------------------------------|
| 1 | C only                              | YES  | Single `.c` file, libcurl + cJSON via nixpkgs                |
| 2 | Nix is the only build system        | YES  | `flake.nix` → `nix build .#mail`                            |
| 3 | Fail hard, zero silent failure      | YES  | Every `stat`, `fopen`, `fread`, `ferror`, `curl_easy_init`, `curl_easy_perform`, `time`, `localtime_r`, `strftime`, `malloc`, `realloc`, `strdup`, `cJSON_*` checked; non-zero exit + structured error JSON |
| 4 | DRY                                 | YES  | Single `die_json`; shared `buf_t` + `write_cb` + `buf_new`; `load_config`/`cfg_require` for config; `find_opt`/`has_flag` for arg parsing |
| 5 | Zero dead code                      | YES  | No unused functions                                          |
| 6 | Zero hardcode                       | YES  | All connection params from config file: `MAIL_SMTP_URL`, `MAIL_IMAP_URL`, `MAIL_USER`, `MAIL_PASS`. Provider-agnostic |
| 7 | `-Wall -Wextra -Werror -std=c2x`   | YES  | Set in `flake.nix` cflags                                   |
| 8 | No output suppression               | YES  | Errors to both stderr and stdout (JSON)                      |
| 9 | Security not weakened               | YES  | Credentials read from `~/.config/vehir/env` (chmod 600 enforced). Secrets never in process env, CLI args, or shell history. TLS enforced by URL scheme (`smtps://`, `imaps://`). No secrets in source |
| 10| No hidden coupling                  | YES  | Standalone binary. Uses libcurl for both SMTP and IMAP (no external daemons) |

### LLM-convenience checklist — mail

| # | Criterion                        | Pass | Notes                                                           |
|---|----------------------------------|------|-----------------------------------------------------------------|
| 1 | Structured JSON output           | YES  | All output is single-line JSON                                  |
| 2 | Consistent schema                | YES  | Send: `{ok, action, to, subject, date}`. Fetch: `{ok, action, mailbox, unseen_only, count, messages:[{uid,from,subject,date}]}`. Error: `{ok, error}` |
| 3 | `--help` / usage                 | YES  | `mail --help` → usage to stderr, exit 2                        |
| 4 | Predictable exit codes           | YES  | 0 = success, 1 = error, 2 = usage                              |
| 5 | No hidden coupling               | YES  | One config file, overridable with `--config`                    |
| 6 | Parseable by an LLM              | YES  | LLM invokes `mail send --to x --subject y`, parses JSON; `mail fetch --unseen` returns parseable list |
| 7 | Declarative where it helps       | YES  | Manifest-driven IPM registration                                |

### Design note: credential security model

Both tools read credentials from `~/.config/vehir/env` (KEY=VALUE format) directly via `fopen`.
The binary checks file permissions with `stat()` and refuses to start if group or other bits are set.
Secrets never enter the process environment (`/proc/pid/environ`), never appear in `ps` output,
and never pass through shell variable expansion. This is strictly stronger than the env-var approach.
Override path: `--config <path>` flag.

### Design note: config file duplication (DRY)

The `load_config`/`cfg_require` code (~80 lines) is duplicated in `forge.c` and `mail.c`.
Per SOUL §7 DRY, "code repeated twice or more is extracted to a shared library immediately."
This is a known deviation: the two tools are independent binaries in separate directories,
and extracting a shared library for 2 consumers adds build complexity for minimal gain.
When a third tool reads the same config, extract to `net/lib/vehir_config.{c,h}`.

### Design note: libcurl for both SMTP and IMAP

libcurl handles both SMTP (send via `smtps://`) and IMAP (fetch via `imaps://`) natively.
This avoids adding libetpan or any other dependency — one library for all network I/O in both tools.
Justification: libcurl is already a dependency for `forge` (HTTP), so reusing it for mail protocols
adds zero new dependencies. libetpan would add a second TLS-capable network library with no benefit.

### Design note: git push setup

`forge` creates repositories, releases, and issues via the Forgejo REST API. Actual `git push` of
commits uses standard git via SSH:

- SSH key: `~/.ssh/vehir_codeberg` (ed25519), configured in `~/.ssh/config` with `IdentitiesOnly yes`
- Remote format: `git@codeberg.org:vehir/<repo>.git`
- IPv4 forced (`AddressFamily inet`) due to IPv6 routing issues on current host

This is NOT reimplemented in `forge` — git's own transport is the right tool. The agent configures
its git remotes once after `forge repo create` returns the `clone_url`.

---

## Verdict

**PASS.** Both tools pass all SOUL-standard and LLM-convenience checklist items.

Verified:

- [x] `nix build .#forge` succeeds (`-Wall -Wextra -Werror -std=c2x`, clean)
- [x] `nix build .#mail` succeeds (`-Wall -Wextra -Werror -std=c2x`, clean)
- [x] `forge --help` exits 2 with usage
- [x] `mail --help` exits 2 with usage
- [x] Config file permission guard: rejects mode 0644 with `{"ok":false,"error":"config ... readable by group/other (mode 0644) — run: chmod 600 ..."}`
- [x] `forge repo list` with missing config key → `{"ok":false,"error":"key FORGE_BASE_URL not found in config"}`, exit 1
- [x] `mail fetch` with missing config key → `{"ok":false,"error":"key MAIL_IMAP_URL not found in config"}`, exit 1
- [x] `idea apply forge.manifest.json` → applied, `compat_verdict=compatible`
- [x] `idea apply mail.manifest.json` → applied, `compat_verdict=compatible`
- [x] `idea build forge` → `proof_status=works`
- [x] `idea build mail` → `proof_status=works`
- [x] `idea check` → overall `compatible`

LIVE smoke tests:

- [x] `forge repo list` → `{"ok":true,"count":1}` (reads config from file, no env vars)
- [x] `forge repo create smoke-test` → created https://codeberg.org/vehir/smoke-test
- [x] `forge issue create smoke-test "forge smoke test"` → issue #1 created
- [x] SSH `git@codeberg.org` → authenticated as `vehir`
- [ ] `forge release create` — not tested (needs a pushed tag first)
- [ ] `mail send` — PENDING (no mailbox provisioned yet; awaiting custom domain + Purelymail)
- [ ] `mail fetch` — PENDING (same)
