# SPDX-License-Identifier: Apache-2.0

{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ], db }:
pkgs.stdenv.mkDerivation {
  pname = "pain"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson db ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -I${db}/include -c pain.c -o pain.o
    ar rcs libpain.a pain.o
    cc ${builtins.toString cflags} -I${db}/include pain_check.c pain.c -o pain-check -lcjson -L${db}/lib -ldb
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib $out/include
    cp pain-check $out/bin/; cp libpain.a $out/lib/; cp pain.h $out/include/
  '';
}
