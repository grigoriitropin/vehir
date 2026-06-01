# SPDX-License-Identifier: Apache-2.0

{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "runtime-paths"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -c runtime_paths.c -o runtime_paths.o
    ar rcs libruntime-paths.a runtime_paths.o
    cc ${builtins.toString cflags} probe.c runtime_paths.c -o runtime-paths-probe -lcjson
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib $out/include
    cp runtime-paths-probe $out/bin/
    cp libruntime-paths.a $out/lib/
    cp runtime_paths.h $out/include/
  '';
}
