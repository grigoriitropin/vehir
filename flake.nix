{
  description = "vehir monorepo — core physics and network packages";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    spec2c.url = "github:grigoriitropin/spec2c";
  };

  outputs = { self, nixpkgs, spec2c }: let
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
        src = ./core/file-age;
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
        src = ./core/runtime-paths;
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
        src = ./core/db;
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
        src = ./core/db-proxy;
        buildInputs = [ pkgs.postgresql pkgs.cjson ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./core/db} \
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
        src = ./core/pain;
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

      git-identity = pkgs.stdenv.mkDerivation {
        pname = "git-identity";
        version = "0.1.0";
        src = ./core/git-identity;
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./core} \
            git_identity.c ${./core}/lib/safe-exec.c -o git-identity
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp git-identity $out/bin/
          runHook postInstall
        '';
      };

      insert-idea-graph-into-readme = pkgs.stdenv.mkDerivation {
        pname = "insert-idea-graph-into-readme";
        version = "0.1.0";
        src = ./core/insert-idea-graph-into-readme;
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./core} \
            insert-idea-graph-into-readme.c ${./core}/lib/safe-exec.c -o insert-idea-graph-into-readme
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp insert-idea-graph-into-readme $out/bin/
          runHook postInstall
        '';
      };

      constitution-validator = pkgs.stdenv.mkDerivation {
        pname = "constitution-validator";
        version = "0.1.0";
        src = ./core/constitution-validator;
        buildInputs = [ pkgs.cjson pkgs.openssl self.packages.${system}.db self.packages.${system}.pain ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            -I${self.packages.${system}.db}/include \
            -I${self.packages.${system}.pain}/include \
            validator.c -o constitution-validator \
            -lcjson -lssl -lcrypto \
            -L${self.packages.${system}.db}/lib -ldb \
            -L${self.packages.${system}.pain}/lib -lpain
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp constitution-validator $out/bin/
          runHook postInstall
        '';
      };

      essentials = pkgs.stdenv.mkDerivation {
        pname = "essentials-daemon";
        version = "0.1.0";
        src = ./core/essentials;
        buildInputs = [ pkgs.cjson pkgs.openssl pkgs.postgresql
                        self.packages.${system}.db self.packages.${system}.pain ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            -I${./core/lib} \
            -I${self.packages.${system}.db}/include \
            -I${self.packages.${system}.pain}/include \
            essentials_daemon_scaffold.c \
            essentials_daemon_core.c \
            ${./core}/lib/vehir_lib.c \
            -o essentials-daemon \
            -lcjson -lssl -lcrypto -lpq \
            -L${self.packages.${system}.db}/lib -ldb \
            -L${self.packages.${system}.pain}/lib -lpain
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp essentials-daemon $out/bin/
          runHook postInstall
        '';
      };

      forge = pkgs.stdenv.mkDerivation {
        pname = "forge";
        version = "0.1.0";
        src = ./net/forge;
        buildInputs = [ pkgs.cjson pkgs.curl ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./core} \
            $(pkg-config --cflags --libs libcurl libcjson) \
            forge.c ${./core}/lib/config-loader.c -o forge
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp forge $out/bin/
          runHook postInstall
        '';
      };

      mail = pkgs.stdenv.mkDerivation {
        pname = "mail";
        version = "0.1.0";
        src = ./net/mail;
        buildInputs = [ pkgs.cjson pkgs.curl ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} -I${./core} \
            $(pkg-config --cflags --libs libcurl libcjson) \
            mail.c ${./core}/lib/config-loader.c -o mail
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp mail $out/bin/
          runHook postInstall
        '';
      };

      default = self.packages.${system}.file-age;
    });
  };
}
