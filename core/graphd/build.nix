{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "graphd"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.postgresql pkgs.cjson ];
  nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = '' cc ${builtins.toString cflags} graphd.c -o graphd -lpq -lcjson '';
  installPhase = '' mkdir -p $out/bin; cp graphd $out/bin/ '';
}
