# SPDX-License-Identifier: Apache-2.0

{
  description = "vehir — core physics and network packages";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    spec2c.url = "github:grigoriitropin/spec2c";
  };

  outputs = { self, nixpkgs, spec2c }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    pkgsFor = system: nixpkgs.legacyPackages.${system};
    cflags = [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ];
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      call = path: args: pkgs.callPackage path ({ inherit cflags; } // args);
      p = self.packages.${system};
    in {
      resolve-secrets-config-from-file       = call ./core/broker/build.nix {};
      check-json-file-timestamp-age          = call ./core/file-age/build.nix {};
      probe-runtime-paths-before-execution   = call ./core/runtime-paths/build.nix {};
      probe-database-through-unix-socket     = call ./core/db/build.nix {};
      gate-sql-via-unix-socket               = call ./core/db-proxy/build.nix {};
      check-pain-write-signals-mode          = call ./core/pain/build.nix { db = p.probe-database-through-unix-socket; };
      declare-git-author-from-config         = call ./core/git-identity/build.nix {};
      insert-idea-graph-into-readme          = call ./core/insert-idea-graph-into-readme/build.nix {};
      generate-idea-graph-event-driven       = call ./core/graphd/build.nix {};
      hash-and-validate-constitution-sections = call ./core/constitution-validator/build.nix {
        db = p.probe-database-through-unix-socket;
        pain = p.check-pain-write-signals-mode;
      };
      listen-change-regenerate-tools-context = call ./core/essentials/build.nix {
        db = p.probe-database-through-unix-socket;
        pain = p.check-pain-write-signals-mode;
      };
      create-forgejo-repos-via-api           = call ./net/forge/build.nix {
        broker = p.resolve-secrets-config-from-file;
      };
      send-and-fetch-email-messages          = call ./net/mail/build.nix {
        broker = p.resolve-secrets-config-from-file;
      };

      default = p.check-json-file-timestamp-age;
    });
  };
}
