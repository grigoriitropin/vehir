{
  description = "vehir-net — networking/identity tools (forge, mail)";

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
      forge = pkgs.stdenv.mkDerivation {
        pname = "forge";
        version = "0.1.0";
        src = ./forge;
        buildInputs = [ pkgs.cjson pkgs.curl ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            $(pkg-config --cflags --libs libcurl libcjson) \
            forge.c -o forge
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
        src = ./mail;
        buildInputs = [ pkgs.cjson pkgs.curl ];
        nativeBuildInputs = [ pkgs.pkg-config ];
        buildPhase = ''
          runHook preBuild
          cc ${builtins.toString cflags} \
            $(pkg-config --cflags --libs libcurl libcjson) \
            mail.c -o mail
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p $out/bin
          cp mail $out/bin/
          runHook postInstall
        '';
      };

      default = self.packages.${system}.forge;
    });
  };
}
