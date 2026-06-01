# SPDX-License-Identifier: Apache-2.0

{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ], db, pain }:
pkgs.stdenv.mkDerivation {
  pname = "constitution-validator"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson pkgs.openssl db pain ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -I${db}/include -I${pain}/include validator.c -o constitution-validator \
      -lcjson -lssl -lcrypto -L${db}/lib -ldb -L${pain}/lib -lpain
  '';
  installPhase = '' mkdir -p $out/bin; cp constitution-validator $out/bin/ '';
}
