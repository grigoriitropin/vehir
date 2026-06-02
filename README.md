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
```mermaid
graph TD
  apply-world-laws-agent-growth["apply-world-laws-agent-growth"]
  resolve-runtime-paths-for-processes["resolve-runtime-paths-for-processes\n[programs: probe-runtime-paths-before-execution(tool)]"]
  regenerate-agent-context-on-change["regenerate-agent-context-on-change\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), listen-change-regenerate-tools-context(service), check-pain-write-signals-mode(tool)]"]
  send-parameterized-sql-over-socket["send-parameterized-sql-over-socket\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service)]"]
  extract-ipm-metadata-to-json["extract-ipm-metadata-to-json\n[programs: extract-ipm-metadata-to-json(tool)]"]
  gate-sql-via-unix-socket["gate-sql-via-unix-socket\n[programs: gate-sql-via-unix-socket(service)]"]
  create-forgejo-repos-via-api["create-forgejo-repos-via-api\n[programs: resolve-secrets-config-from-file(library), create-forgejo-repos-via-api(tool)]"]
  declare-git-author-from-config["declare-git-author-from-config\n[programs: declare-git-author-from-config(tool)]"]
  hash-and-validate-constitution-sections["hash-and-validate-constitution-sections\n[programs: hash-and-validate-constitution-sections(tool), probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), check-pain-write-signals-mode(tool)]"]
  scan-filesystem-with-nftw-walk["scan-filesystem-with-nftw-walk\n[programs: scan-filesystem-with-nftw-walk(tool)]"]
  rerank-memory-with-neural-attention["rerank-memory-with-neural-attention\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service), rescore-memory-with-msa-attention(service)]"]
  auto-switch-embed-rerank-gpu["auto-switch-embed-rerank-gpu\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service), rescore-memory-with-msa-attention(service), auto-switch-embed-rerank-gpu(service)]"]
  resolve-configuration-secrets-from-file["resolve-configuration-secrets-from-file\n[programs: resolve-secrets-config-from-file(library)]"]
  build-file-dependency-graph-json["build-file-dependency-graph-json\n[programs: build-file-dependency-graph-json(tool)]"]
  generate-idea-graph-event-driven["generate-idea-graph-event-driven\n[programs: generate-idea-graph-event-driven(service)]"]
  audit-directory-and-count-lines["audit-directory-and-count-lines\n[programs: audit-directory-and-count-lines(tool)]"]
  compile-ipm-specifications-to-c["compile-ipm-specifications-to-c\n[programs: audit-directory-and-count-lines(tool), compile-ipm-specifications-to-c(tool), extract-ipm-metadata-to-json(tool)]"]
  convert-text-to-dense-vectors["convert-text-to-dense-vectors\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), generate-dense-vectors-with-qwen3(service)]"]
  write-pain-signals-to-registry["write-pain-signals-to-registry\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), check-pain-write-signals-mode(tool)]"]
  check-json-file-timestamp-age["check-json-file-timestamp-age\n[programs: check-json-file-timestamp-age(tool)]"]
  check-license-compatibility-across-graph["check-license-compatibility-across-graph\n[programs: probe-database-through-unix-socket(tool), gate-sql-via-unix-socket(service), check-license-compatibility-across-graph(tool)]"]
  send-and-fetch-email-messages["send-and-fetch-email-messages\n[programs: resolve-secrets-config-from-file(library), send-and-fetch-email-messages(tool)]"]
  insert-idea-graph-into-readme["insert-idea-graph-into-readme\n[programs: insert-idea-graph-into-readme(tool)]"]
  apply-world-laws-agent-growth -->|"Documentation graph generator — keeps the package graph visible and up to date so agents and humans can see the system state at a glance\n(Vehir)"| insert-idea-graph-into-readme
  apply-world-laws-agent-growth -->|"Pain signal system — cognitive mode switching based on system health, providing the feedback loop that makes the enforcement architecture a real physics\n(Vehir)"| write-pain-signals-to-registry
  apply-world-laws-agent-growth -->|"Database access substrate — all agent memory and state lives in PostgreSQL, accessed through a parameterized query interface with strict error handling\n(Vehir)"| send-parameterized-sql-over-socket
  apply-world-laws-agent-growth -->|"Foundational path resolution — every tool needs to find its config, sockets, and data directories at runtime without hardcoded paths\n(Vehir)"| resolve-runtime-paths-for-processes
  apply-world-laws-agent-growth -->|"Secrets and configuration broker — every tool resolves credentials by name through one audited layer, never from environment or command line\n(Vehir)"| resolve-configuration-secrets-from-file
  auto-switch-embed-rerank-gpu -->|"Reads and writes memory entries through the db client library for vector data during embedding and reranking operations\n(Vehir)"| send-parameterized-sql-over-socket
  auto-switch-embed-rerank-gpu -->|"Wraps MSA-4B rerank model loaded on demand via a spawned worker process for neural rescoring of retrieval results\n(Vehir)"| rerank-memory-with-neural-attention
  rerank-memory-with-neural-attention -->|"Reads memory entries from the database through the db client library for document prefill before neural reranking\n(Vehir)"| send-parameterized-sql-over-socket
  rerank-memory-with-neural-attention -->|"Receives dense vector embeddings from the Qwen3 embedding service as the initial retrieval stage before applying multi-scale attention rescoring\n(Vehir)"| convert-text-to-dense-vectors
  regenerate-agent-context-on-change -->|"Signals tool context drift events to the pain registry when a SHA256 mismatch is detected against the known baseline\n(Vehir)"| write-pain-signals-to-registry
  send-parameterized-sql-over-socket -->|"Routes SQL queries from the db client library over a Unix socket to the db-proxy daemon for validation and execution\n(Vehir)"| gate-sql-via-unix-socket
  write-pain-signals-to-registry -->|"Routes SQL statements through the db client library to write pain events and read cognitive mode from the database\n(Vehir)"| send-parameterized-sql-over-socket
  hash-and-validate-constitution-sections -->|"Signals pain events on detected SOUL.md section drift — writes severity and description through pain library\n(Vehir)"| write-pain-signals-to-registry
  hash-and-validate-constitution-sections -->|"Reads and writes soul_hashes table through the db client library for drift detection and re-baseline operations\n(Vehir)"| send-parameterized-sql-over-socket
  create-forgejo-repos-via-api -->|"Reads Forgejo API token and base URL from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
  regenerate-agent-context-on-change -->|"Stores the regenerated tool context hash in the soul_hashes table through the db-proxy gateway for drift detection\n(Vehir)"| send-parameterized-sql-over-socket
  check-license-compatibility-across-graph -->|"Queries the data store through the db client library for license compatibility cross-checking\n(Vehir)"| send-parameterized-sql-over-socket
  compile-ipm-specifications-to-c -->|"ipm meta tool reads dot ipm specification format defined by spec2c compiler\n(Vehir)"| extract-ipm-metadata-to-json
  compile-ipm-specifications-to-c -->|"spec2c compiles ipm audit tool into native c binary\n(Vehir)"| audit-directory-and-count-lines
  convert-text-to-dense-vectors -->|"Stores generated dense vector embeddings in the memory entries table through the db client library for later retrieval and reranking\n(Vehir)"| send-parameterized-sql-over-socket
  auto-switch-embed-rerank-gpu -->|"Wraps Qwen3-Embedding-4B model running as a resident embed worker for always-on dense vector generation\n(Vehir)"| convert-text-to-dense-vectors
  send-and-fetch-email-messages -->|"Reads SMTP credentials and IMAP connection settings from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
```
<!-- /vehir:graph -->

### Ideas

<!-- vehir:graph:ideas -->
```mermaid
graph TD
  apply-world-laws-agent-growth["apply-world-laws-agent-growth\n(Vehir)\n[pending]\n\nAn agent develops real capability only inside a world whose laws are rich and cannot be bent — the same way intelligence in the real world is shaped by a physics it cannot cheat. The enforcement, isolation, and coding laws below are not security overhead; they ARE that physics. Strengthening the law-substrate raises the agent's ceiling. This axiom is the root; everything else is justified by it."]
  resolve-runtime-paths-for-processes["resolve-runtime-paths-for-processes\n(Vehir)\n[pending]"]
  regenerate-agent-context-on-change["regenerate-agent-context-on-change\n(Vehir)\n[pending]"]
  send-parameterized-sql-over-socket["send-parameterized-sql-over-socket\n(Vehir)\n[pending]"]
  extract-ipm-metadata-to-json["extract-ipm-metadata-to-json\n(Vehir)\n[pending]"]
  gate-sql-via-unix-socket["gate-sql-via-unix-socket\n(Vehir)\n[pending]"]
  create-forgejo-repos-via-api["create-forgejo-repos-via-api\n(Vehir)\n[pending]"]
  declare-git-author-from-config["declare-git-author-from-config\n(Vehir)\n[pending]"]
  hash-and-validate-constitution-sections["hash-and-validate-constitution-sections\n(Vehir)\n[pending]"]
  scan-filesystem-with-nftw-walk["scan-filesystem-with-nftw-walk\n(Vehir)\n[pending]"]
  rerank-memory-with-neural-attention["rerank-memory-with-neural-attention\n(Vehir)\n[pending]"]
  auto-switch-embed-rerank-gpu["auto-switch-embed-rerank-gpu\n(Vehir)\n[pending]"]
  resolve-configuration-secrets-from-file["resolve-configuration-secrets-from-file\n(Vehir)\n[pending]"]
  build-file-dependency-graph-json["build-file-dependency-graph-json\n(Vehir)\n[pending]"]
  generate-idea-graph-event-driven["generate-idea-graph-event-driven\n(Vehir)\n[pending]"]
  audit-directory-and-count-lines["audit-directory-and-count-lines\n(Vehir)\n[pending]"]
  compile-ipm-specifications-to-c["compile-ipm-specifications-to-c\n(Vehir + Grigorii Tropin)\n[pending]"]
  convert-text-to-dense-vectors["convert-text-to-dense-vectors\n(Vehir)\n[pending]"]
  write-pain-signals-to-registry["write-pain-signals-to-registry\n(Vehir)\n[pending]"]
  check-json-file-timestamp-age["check-json-file-timestamp-age\n(Vehir)\n[pending]"]
  check-license-compatibility-across-graph["check-license-compatibility-across-graph\n(Vehir)\n[pending]"]
  send-and-fetch-email-messages["send-and-fetch-email-messages\n(Vehir)\n[pending]"]
  insert-idea-graph-into-readme["insert-idea-graph-into-readme\n(Vehir)\n[pending]"]
  apply-world-laws-agent-growth -->|"Documentation graph generator — keeps the package graph visible and up to date so agents and humans can see the system state at a glance\n(Vehir)"| insert-idea-graph-into-readme
  apply-world-laws-agent-growth -->|"Pain signal system — cognitive mode switching based on system health, providing the feedback loop that makes the enforcement architecture a real physics\n(Vehir)"| write-pain-signals-to-registry
  apply-world-laws-agent-growth -->|"Database access substrate — all agent memory and state lives in PostgreSQL, accessed through a parameterized query interface with strict error handling\n(Vehir)"| send-parameterized-sql-over-socket
  apply-world-laws-agent-growth -->|"Foundational path resolution — every tool needs to find its config, sockets, and data directories at runtime without hardcoded paths\n(Vehir)"| resolve-runtime-paths-for-processes
  apply-world-laws-agent-growth -->|"Secrets and configuration broker — every tool resolves credentials by name through one audited layer, never from environment or command line\n(Vehir)"| resolve-configuration-secrets-from-file
  auto-switch-embed-rerank-gpu -->|"Reads and writes memory entries through the db client library for vector data during embedding and reranking operations\n(Vehir)"| send-parameterized-sql-over-socket
  auto-switch-embed-rerank-gpu -->|"Wraps MSA-4B rerank model loaded on demand via a spawned worker process for neural rescoring of retrieval results\n(Vehir)"| rerank-memory-with-neural-attention
  rerank-memory-with-neural-attention -->|"Reads memory entries from the database through the db client library for document prefill before neural reranking\n(Vehir)"| send-parameterized-sql-over-socket
  rerank-memory-with-neural-attention -->|"Receives dense vector embeddings from the Qwen3 embedding service as the initial retrieval stage before applying multi-scale attention rescoring\n(Vehir)"| convert-text-to-dense-vectors
  regenerate-agent-context-on-change -->|"Signals tool context drift events to the pain registry when a SHA256 mismatch is detected against the known baseline\n(Vehir)"| write-pain-signals-to-registry
  send-parameterized-sql-over-socket -->|"Routes SQL queries from the db client library over a Unix socket to the db-proxy daemon for validation and execution\n(Vehir)"| gate-sql-via-unix-socket
  write-pain-signals-to-registry -->|"Routes SQL statements through the db client library to write pain events and read cognitive mode from the database\n(Vehir)"| send-parameterized-sql-over-socket
  hash-and-validate-constitution-sections -->|"Signals pain events on detected SOUL.md section drift — writes severity and description through pain library\n(Vehir)"| write-pain-signals-to-registry
  hash-and-validate-constitution-sections -->|"Reads and writes soul_hashes table through the db client library for drift detection and re-baseline operations\n(Vehir)"| send-parameterized-sql-over-socket
  create-forgejo-repos-via-api -->|"Reads Forgejo API token and base URL from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
  regenerate-agent-context-on-change -->|"Stores the regenerated tool context hash in the soul_hashes table through the db-proxy gateway for drift detection\n(Vehir)"| send-parameterized-sql-over-socket
  check-license-compatibility-across-graph -->|"Queries the data store through the db client library for license compatibility cross-checking\n(Vehir)"| send-parameterized-sql-over-socket
  compile-ipm-specifications-to-c -->|"ipm meta tool reads dot ipm specification format defined by spec2c compiler\n(Vehir)"| extract-ipm-metadata-to-json
  compile-ipm-specifications-to-c -->|"spec2c compiles ipm audit tool into native c binary\n(Vehir)"| audit-directory-and-count-lines
  convert-text-to-dense-vectors -->|"Stores generated dense vector embeddings in the memory entries table through the db client library for later retrieval and reranking\n(Vehir)"| send-parameterized-sql-over-socket
  auto-switch-embed-rerank-gpu -->|"Wraps Qwen3-Embedding-4B model running as a resident embed worker for always-on dense vector generation\n(Vehir)"| convert-text-to-dense-vectors
  send-and-fetch-email-messages -->|"Reads SMTP credentials and IMAP connection settings from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-configuration-secrets-from-file
```
<!-- /vehir:graph -->

### Programs

<!-- vehir:graph:programs -->
```mermaid
graph TD
  audit-directory-and-count-lines["audit-directory-and-count-lines v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  auto-switch-embed-rerank-gpu["auto-switch-embed-rerank-gpu v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  build-file-dependency-graph-json["build-file-dependency-graph-json v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  check-json-file-timestamp-age["check-json-file-timestamp-age v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  check-license-compatibility-across-graph["check-license-compatibility-across-graph v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  check-pain-write-signals-mode["check-pain-write-signals-mode v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  cjson["cjson v0\n[pending]\n[license: unchecked]\nMIT\nDave Gamble"]
  compile-ipm-specifications-to-c["compile-ipm-specifications-to-c v0.2.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir + Grigorii Tropin"]
  create-forgejo-repos-via-api["create-forgejo-repos-via-api v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  declare-git-author-from-config["declare-git-author-from-config v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  extract-ipm-metadata-to-json["extract-ipm-metadata-to-json v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  gate-sql-via-unix-socket["gate-sql-via-unix-socket v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  generate-dense-vectors-with-qwen3["generate-dense-vectors-with-qwen3 v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  generate-idea-graph-event-driven["generate-idea-graph-event-driven v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  git["git v0\n[pending]\n[license: unchecked]\nGPL-2.0-only\nGit community"]
  glibc["glibc v0\n[pending]\n[license: unchecked]\nLGPL-2.1-only\nGNU Project"]
  hash-and-validate-constitution-sections["hash-and-validate-constitution-sections v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  insert-idea-graph-into-readme["insert-idea-graph-into-readme v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  ipm["ipm v0.1.6\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  libpq["libpq v0\n[pending]\n[license: unchecked]\nPostgreSQL\nPostgreSQL Global"]
  listen-change-regenerate-tools-context["listen-change-regenerate-tools-context v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  nix["nix v0\n[pending]\n[license: unchecked]\nLGPL-2.1-only\nNixOS Foundation"]
  openssl["openssl v0\n[pending]\n[license: unchecked]\nApache-2.0\nOpenSSL Project"]
  postgres["postgres v0\n[pending]\n[license: unchecked]\nPostgreSQL\nPostgreSQL Global"]
  probe-database-through-unix-socket["probe-database-through-unix-socket v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  probe-runtime-paths-before-execution["probe-runtime-paths-before-execution v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  rescore-memory-with-msa-attention["rescore-memory-with-msa-attention v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  resolve-secrets-config-from-file["resolve-secrets-config-from-file v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  scan-filesystem-with-nftw-walk["scan-filesystem-with-nftw-walk v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  send-and-fetch-email-messages["send-and-fetch-email-messages v0.1.0\n[pending]\n[license: unchecked]\nApache-2.0\nVehir"]
  auto-switch-embed-rerank-gpu -->|"Wraps Qwen3-Embedding-4B model running as a resident embed worker for always-on dense vector generation\n(Vehir)"| generate-dense-vectors-with-qwen3
  auto-switch-embed-rerank-gpu -->|"Reads and writes memory entries through the db client library for vector data during embedding and reranking operations\n(Vehir)"| probe-database-through-unix-socket
  auto-switch-embed-rerank-gpu -->|"Wraps MSA-4B rerank model loaded on demand via a spawned worker process for neural rescoring of retrieval results\n(Vehir)"| rescore-memory-with-msa-attention
  check-license-compatibility-across-graph -->|"Queries the data store through the db client library for license compatibility cross-checking\n(Vehir)"| probe-database-through-unix-socket
  check-pain-write-signals-mode -->|"Routes SQL statements through the db client library to write pain events and read cognitive mode from the database\n(Vehir)"| probe-database-through-unix-socket
  compile-ipm-specifications-to-c -->|"spec2c compiles ipm audit tool into native c binary\n(Vehir)"| audit-directory-and-count-lines
  compile-ipm-specifications-to-c -->|"ipm meta tool reads dot ipm specification format defined by spec2c compiler\n(Vehir)"| extract-ipm-metadata-to-json
  create-forgejo-repos-via-api -->|"Reads Forgejo API token and base URL from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-secrets-config-from-file
  generate-dense-vectors-with-qwen3 -->|"Stores generated dense vector embeddings in the memory entries table through the db client library for later retrieval and reranking\n(Vehir)"| probe-database-through-unix-socket
  hash-and-validate-constitution-sections -->|"Signals pain events on detected SOUL.md section drift — writes severity and description through pain library\n(Vehir)"| check-pain-write-signals-mode
  hash-and-validate-constitution-sections -->|"Reads and writes soul_hashes table through the db client library for drift detection and re-baseline operations\n(Vehir)"| probe-database-through-unix-socket
  listen-change-regenerate-tools-context -->|"Signals tool context drift events to the pain registry when a SHA256 mismatch is detected against the known baseline\n(Vehir)"| check-pain-write-signals-mode
  listen-change-regenerate-tools-context -->|"Stores the regenerated tool context hash in the soul_hashes table through the db-proxy gateway for drift detection\n(Vehir)"| probe-database-through-unix-socket
  probe-database-through-unix-socket -->|"Routes SQL queries from the db client library over a Unix socket to the db-proxy daemon for validation and execution\n(Vehir)"| gate-sql-via-unix-socket
  rescore-memory-with-msa-attention -->|"Receives dense vector embeddings from the Qwen3 embedding service as the initial retrieval stage before applying multi-scale attention rescoring\n(Vehir)"| generate-dense-vectors-with-qwen3
  rescore-memory-with-msa-attention -->|"Reads memory entries from the database through the db client library for document prefill before neural reranking\n(Vehir)"| probe-database-through-unix-socket
  send-and-fetch-email-messages -->|"Reads SMTP credentials and IMAP connection settings from the secrets broker — zero secrets in environment or CLI\n(Vehir)"| resolve-secrets-config-from-file
  check-license-compatibility-across-graph -->|"Links cJSON for structured JSON output of license compliance reports.\n(Vehir)"| cjson
  check-license-compatibility-across-graph -->|"Dynamic-link glibc for standard C runtime interfaces — validates against the full transitive dependency closure for deterministic license compatibility enforcement.\n(Vehir)"| glibc
  check-license-compatibility-across-graph -->|"Queries the IPM dependency graph via libpq for license compatibility cross-checking.\n(Vehir)"| libpq
  check-license-compatibility-across-graph -->|"Links OpenSSL for SHA256 hash computation when verifying LICENSE file integrity.\n(Vehir)"| openssl
  git -->|"IPM tracks every package through git for provenance — every manifest lives in a repository, every build is tied to a commit.\n(Vehir)"| ipm
  hash-and-validate-constitution-sections -->|"Uses OpenSSL for SHA256 hashing of constitution section content — linked via libssl and libcrypto.\n(Vehir)"| openssl
  listen-change-regenerate-tools-context -->|"JSON parsing of idea show output for tool context block regeneration.\n(Vehir)"| cjson
  nix -->|"IPM builds every program through Nix flakes — no global installs, fully reproducible, dependencies pinned.\n(Vehir)"| ipm
  postgres -->|"IPM stores the entire idea graph in PostgreSQL — ideas, programs, licenses, edges, and compat cache all live in one transactional store over libpq.\n(Vehir)"| ipm
```
<!-- /vehir:graph -->

