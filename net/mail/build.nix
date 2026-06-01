# SPDX-License-Identifier: Apache-2.0

{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ], broker }:
pkgs.stdenv.mkDerivation {
  pname = "mail";
  version = "0.1.0";
  src = ./.;
  buildInputs = [ pkgs.cjson pkgs.curl broker ];
  nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} \
      -I${broker}/include \
      $(pkg-config --cflags --libs libcurl libcjson) \
      mail.c -o mail \
      -L${broker}/lib -lvehir-config
  '';
  installPhase = ''
    mkdir -p $out/bin
    cp mail $out/bin/
  '';
}
