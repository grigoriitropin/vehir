# SPDX-License-Identifier: Apache-2.0

{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ], broker }:
pkgs.stdenv.mkDerivation {
  pname = "forge";
  version = "0.1.0";
  src = ./.;
  buildInputs = [ pkgs.cjson pkgs.curl broker ];
  nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} \
      -I${broker}/include \
      $(pkg-config --cflags --libs libcurl libcjson) \
      forge.c -o forge \
      -L${broker}/lib -lvehir-config
  '';
  installPhase = ''
    mkdir -p $out/bin
    cp forge $out/bin/
  '';
}
