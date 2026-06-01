{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "db-proxy"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.postgresql pkgs.cjson ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = '' cc ${builtins.toString cflags} -I${./../db} db_proxy.c -o db-proxy -lpq -lcjson '';
  installPhase = '' mkdir -p $out/bin; cp db-proxy $out/bin/ '';
}
