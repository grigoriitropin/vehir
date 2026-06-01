# Vehir — Core Operational Physics

Package graph of LLM agent runtime. Managed by [IPM](https://github.com/grigoriitropin/IPM).

## Architecture

- **Profile manager** — atomic generation symlink-farm, isolated per workspace
- **ipm-loader** — Stage 1 anti-brick, rollback works even if core is dead
- **Pre-flight check** — binary verified before every activation
- **Embedded SQL migrations** — advisory lock, version drift barrier
- **Audit journal** — append-only JSONL, auto-purge 30 days

## Packages

| Package | Type | Status |
|---|---|---|
| broker | library | ✅ compiles |
| file-age | tool | ✅ PASS |
| runtime-paths | library | ✅ PASS |
| db | library | ✅ PASS |
| db-proxy | service | ✅ PASS |
| forge | tool | ✅ compiles |
| mail | tool | ✅ compiles |
| pain | library | 🔶 coded |
| git-identity | tool | 🔶 coded |
| readme-gen | tool | ✅ compiles |
| constitution-validator | tool | 🔶 coded |
| essentials | service | 🔶 coded |

## Graphs

### Combined
# Graph
```mermaid
graph TD
```

### Ideas
<!-- IDEA-GRAPH: ideas -->

### Programs
<!-- IDEA-GRAPH: programs -->

