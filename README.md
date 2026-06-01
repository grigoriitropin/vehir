# Vehir — Core Operational Physics

Package graph of LLM agent runtime. Managed by [IPM](https://github.com/grigoriitropin/IPM).

## Architecture

- **Profile manager** — atomic generation symlink-farm, isolated per workspace
- **ipm-loader** — Stage 1 anti-brick, rollback works even if core is dead
- **Pre-flight check** — binary verified before every activation
- **Embedded SQL migrations** — advisory lock, version drift barrier
- **Audit journal** — append-only JSONL, auto-purge 30 days

## Graph

```mermaid
<!-- vehir:graph:combined -->
graph TD
  axiom["axiom"]
  apply-world-laws-agent-growth["apply-world-laws-agent-growth"]
  declare-git-author-from-config["declare-git-author-from-config\n[programs: declare-git-author-from-config(tool)]"]
  generate-idea-graph-event-driven["generate-idea-graph-event-driven\n[programs: generate-idea-graph-event-driven(service)]"]
  write-pain-signals-to-registry["write-pain-signals-to-registry\n[programs: check-pain-write-signals-mode(tool), gate-sql-via-unix-socket(service), probe-database-through-unix-socket(tool)]"]
  auto-switch-embed-rerank-gpu["auto-switch-embed-rerank-gpu\n[programs: auto-switch-embed-rerank-gpu(service), rescore-memory-with-msa-attention(service), gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service), probe-database-through-unix-socket(tool)]"]
  create-forgejo-repos-via-api["create-forgejo-repos-via-api\n[programs: create-forgejo-repos-via-api(tool), resolve-secrets-config-from-file(library)]"]
  resolve-configuration-secrets-from-file["resolve-configuration-secrets-from-file\n[programs: resolve-secrets-config-from-file(library)]"]
  resolve-runtime-paths-for-processes["resolve-runtime-paths-for-processes\n[programs: probe-runtime-paths-before-execution(tool)]"]
  ipm["ipm\n[programs: ipm(tool)]"]
  send-and-fetch-email-messages["send-and-fetch-email-messages\n[programs: resolve-secrets-config-from-file(library), send-and-fetch-email-messages(tool)]"]
  hash-and-validate-constitution-sections["hash-and-validate-constitution-sections\n[programs: check-pain-write-signals-mode(tool), hash-and-validate-constitution-sections(tool), gate-sql-via-unix-socket(service), probe-database-through-unix-socket(tool)]"]
  convert-text-to-dense-vectors["convert-text-to-dense-vectors\n[programs: gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service), probe-database-through-unix-socket(tool)]"]
  insert-idea-graph-into-readme["insert-idea-graph-into-readme\n[programs: insert-idea-graph-into-readme(tool)]"]
  regenerate-agent-context-on-change["regenerate-agent-context-on-change\n[programs: check-pain-write-signals-mode(tool), gate-sql-via-unix-socket(service), listen-change-regenerate-tools-context(service), probe-database-through-unix-socket(tool)]"]
  rerank-memory-with-neural-attention["rerank-memory-with-neural-attention\n[programs: rescore-memory-with-msa-attention(service), gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service), probe-database-through-unix-socket(tool)]"]
  check-license-compatibility-across-graph["check-license-compatibility-across-graph\n[programs: check-license-compatibility-across-graph(tool), gate-sql-via-unix-socket(service), probe-database-through-unix-socket(tool)]"]
  send-parameterized-sql-over-socket["send-parameterized-sql-over-socket\n[programs: gate-sql-via-unix-socket(service), probe-database-through-unix-socket(tool)]"]
  check-json-file-timestamp-age["check-json-file-timestamp-age\n[programs: check-json-file-timestamp-age(tool)]"]
  gate-sql-via-unix-socket["gate-sql-via-unix-socket\n[programs: gate-sql-via-unix-socket(service)]"]
  apply-world-laws-agent-growth -->|"Secrets and configuration broker — every tool resolves credentials by name through one audited layer, never from environment or command line\n(Vehir)"| resolve-configuration-secrets-from-file
  apply-world-laws-agent-growth -->|"Foundational path resolution — every tool needs to find its config, sockets, and data directories at runtime without hardcoded paths\n(Vehir)"| resolve-runtime-paths-for-processes
  axiom -->|"The axiom depends on a declarative package manager to enforce structure, provenance, and deterministic compatibility — IPM is the concrete implementation of that principle.\n(Grigorii Tropin)"| ipm
  apply-world-laws-agent-growth -->|"Documentation graph generator — keeps the package graph visible and up to date so agents and humans can see the system state at a glance\n(Vehir)"| insert-idea-graph-into-readme
  apply-world-laws-agent-growth -->|"Pain signal system — cognitive mode switching based on system health, providing the feedback loop that makes the enforcement architecture a real physics\n(Vehir)"| write-pain-signals-to-registry
  apply-world-laws-agent-growth -->|"Database access substrate — all agent memory and state lives in PostgreSQL, accessed through a parameterized query interface with strict error handling\n(Vehir)"| send-parameterized-sql-over-socket
  check-license-compatibility-across-graph -->|"Queries the data store through the db client library for license compatibility cross-checking\n(Vehir)"| send-parameterized-sql-over-socket
  send-parameterized-sql-over-socket -->|"Routes SQL queries from the db client library over a Unix socket to the db-proxy daemon for validation and execution\n(Vehir)"| gate-sql-via-unix-socket
  auto-switch-embed-rerank-gpu -->|"Wraps MSA-4B rerank model loaded on demand via a spawned worker process for neural rescoring of retrieval results\n(Vehir)"| rerank-memory-with-neural-attention
  auto-switch-embed-rerank-gpu -->|"Wraps Qwen3-Embedding-4B model running as a resident embed worker for always-on dense vector generation\n(Vehir)"| convert-text-to-dense-vectors
  send-and-fetch-email-messages -->|"Reads SMTP credentials and IMAP connection settings from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
  create-forgejo-repos-via-api -->|"Reads Forgejo API token and base URL from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
  rerank-memory-with-neural-attention -->|"Reads memory entries from the database through the db client library for document prefill before neural reranking\n(Vehir)"| send-parameterized-sql-over-socket
  convert-text-to-dense-vectors -->|"Stores generated dense vector embeddings in the memory entries table through the db client library for later retrieval and reranking\n(Vehir)"| send-parameterized-sql-over-socket
  regenerate-agent-context-on-change -->|"Signals tool context drift events to the pain registry when a SHA256 mismatch is detected against the known baseline\n(Vehir)"| write-pain-signals-to-registry
  regenerate-agent-context-on-change -->|"Stores the regenerated tool context hash in the soul_hashes table through the db-proxy gateway for drift detection\n(Vehir)"| send-parameterized-sql-over-socket
  hash-and-validate-constitution-sections -->|"Signals pain events on detected SOUL.md section drift — writes severity and description through pain library\n(Vehir)"| write-pain-signals-to-registry
  auto-switch-embed-rerank-gpu -->|"Reads and writes memory entries through the db client library for vector data during embedding and reranking operations\n(Vehir)"| send-parameterized-sql-over-socket
  hash-and-validate-constitution-sections -->|"Reads and writes soul_hashes table through the db client library for drift detection and re-baseline operations\n(Vehir)"| send-parameterized-sql-over-socket
  write-pain-signals-to-registry -->|"Routes SQL statements through the db client library to write pain events and read cognitive mode from the database\n(Vehir)"| send-parameterized-sql-over-socket
  rerank-memory-with-neural-attention -->|"Receives dense vector embeddings from the Qwen3 embedding service as the initial retrieval stage before applying multi-scale attention rescoring\n(Vehir)"| convert-text-to-dense-vectors
<!-- /vehir:graph -->
```

### Ideas

```mermaid
<!-- vehir:graph:ideas -->
<!-- /vehir:graph -->
```

### Programs

```mermaid
<!-- vehir:graph:programs -->
<!-- /vehir:graph -->
```

