{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "db"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -c db.c -o db.o
    ar rcs libdb.a db.o
    cc ${builtins.toString cflags} db_probe.c db.c -o db-probe -lcjson
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib $out/include
    cp db-probe $out/bin/; cp libdb.a $out/lib/; cp db.h $out/include/
  '';
}
