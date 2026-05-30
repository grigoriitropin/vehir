{
  description = "vehir-core — runtime/physics packages";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" "aarch64-linux" ];
    pkgsFor = system: nixpkgs.legacyPackages.${system};
  in {
    packages = forAllSystems (system: let
      pkgs = pkgsFor system;
      cflags = [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ];
    in {
      file-age = pkgs.stdenv.mkDerivation {
        pname = "file-age";
        version = "0.1.0";
        src = ./file-age;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            file_age.c -o file-age -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp file-age $out/bin/
          runHook postInstall
        '';
      };

      runtime-paths = pkgs.stdenv.mkDerivation {
        pname = "runtime-paths";
        version = "0.1.0";
        src = ./runtime-paths;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -c runtime_paths.c -o runtime_paths.o
          ar rcs libruntime-paths.a runtime_paths.o
          cc ${builtins.toString cflags} \
            probe.c runtime_paths.c -o runtime-paths-probe -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/lib $out/include
          cp runtime-paths-probe $out/bin/
          cp libruntime-paths.a  $out/lib/
          cp runtime_paths.h     $out/include/
          runHook postInstall
        '';
      };

      db = pkgs.stdenv.mkDerivation {
        pname = "db";
        version = "0.1.0";
        src = ./db;
        buildInputs = [ pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -c db.c -o db.o
          ar rcs libdb.a db.o
          cc ${builtins.toString cflags} \
            db_probe.c db.c -o db-probe -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/lib $out/include
          cp db-probe $out/bin/
          cp libdb.a  $out/lib/
          cp db.h     $out/include/
          runHook postInstall
        '';
      };

      db-proxy = pkgs.stdenv.mkDerivation {
        pname = "db-proxy";
        version = "0.1.0";
        src = ./db-proxy;
        buildInputs = [ pkgs.postgresql pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./db} \
            db_proxy.c -o db-proxy -lpq -lcjson
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp db-proxy $out/bin/
          runHook postInstall
        '';
      };

      pain = pkgs.stdenv.mkDerivation {
        pname = "pain";
        version = "0.1.0";
        src = ./pain;
        buildInputs = [ pkgs.cjson self.packages.${system}.db ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${self.packages.${system}.db}/include \
            -c pain.c -o pain.o
          ar rcs libpain.a pain.o
          cc ${builtins.toString cflags} -I${self.packages.${system}.db}/include \
            pain_check.c pain.c -o pain-check -lcjson -L${self.packages.${system}.db}/lib -ldb
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin $out/lib $out/include
          cp pain-check $out/bin/
          cp libpain.a  $out/lib/
          cp pain.h     $out/include/
          runHook postInstall
        '';
      };

      default = self.packages.${system}.file-age;
    });
  };
}
