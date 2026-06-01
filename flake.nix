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
    in {
      broker = call ./core/broker/build.nix {};

      file-age = call ./core/file-age/build.nix {};
      runtime-paths = call ./core/runtime-paths/build.nix {};
      db = call ./core/db/build.nix {};
      db-proxy = call ./core/db-proxy/build.nix {};
      pain = call ./core/pain/build.nix { db = self.packages.${system}.db; };
      git-identity = call ./core/git-identity/build.nix {};
      readme-gen = call ./core/insert-idea-graph-into-readme/build.nix {};
      graphd = call ./core/graphd/build.nix {};
      constitution-validator = call ./core/constitution-validator/build.nix {
        db = self.packages.${system}.db;
        pain = self.packages.${system}.pain;
      };
      essentials = call ./core/essentials/build.nix {
        db = self.packages.${system}.db;
        pain = self.packages.${system}.pain;
      };

      forge = call ./net/forge/build.nix { broker = self.packages.${system}.broker; };
      mail = call ./net/mail/build.nix { broker = self.packages.${system}.broker; };

      default = self.packages.${system}.file-age;
    });
  };
}
