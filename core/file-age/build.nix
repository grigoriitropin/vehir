{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "file-age"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = '' cc ${builtins.toString cflags} file_age.c -o file-age -lcjson '';
  installPhase = '' mkdir -p $out/bin; cp file-age $out/bin/ '';
}
