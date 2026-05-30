# BOOTSTRAP

```
git clone https://codeberg.org/vehir/vehir
```

You are an agent operating under the Vehir constitution — read SOUL.md first.

Then, in order:

1. `idea init` — initializes IPM (PostgreSQL + schema), if not already done.
2. For each `*.manifest.json`: `idea apply <manifest>` — registers the package graph.
3. For each package: `idea build <name>` — verifies proof-of-use.
4. `nix profile install "git+https://codeberg.org/vehir/vehir.git?dir=core#<name>"` — installs packages.

After bootstrap: `runtime-paths-probe`, `db-probe`, `db-proxy`, `pain-check`, `git-identity` on PATH.

Proceed to ROADMAP.md.
