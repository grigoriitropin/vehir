{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "insert-idea-graph-into-readme"; version = "0.1.0"; src = ./.;
  buildPhase = '' cc ${builtins.toString cflags} readme-gen.c -o insert-idea-graph-into-readme '';
  installPhase = '' mkdir -p $out/bin; cp insert-idea-graph-into-readme $out/bin/ '';
}
