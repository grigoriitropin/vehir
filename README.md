# Vehir — Core Operational Physics

Package graph of LLM agent runtime. Managed by [IPM](https://github.com/grigoriitropin/IPM).

## Architecture

- **Profile manager** — atomic generation symlink-farm, isolated per workspace
- **ipm-loader** — Stage 1 anti-brick, rollback works even if core is dead
- **Pre-flight check** — binary verified before every activation
- **Embedded SQL migrations** — advisory lock, version drift barrier
- **Audit journal** — append-only JSONL, auto-purge 30 days

## Graph

<!-- vehir:graph:combined -->
<!-- /vehir:graph -->

### Ideas

<!-- vehir:graph:ideas -->
<!-- /vehir:graph -->

### Programs

<!-- vehir:graph:programs -->
<!-- /vehir:graph -->

