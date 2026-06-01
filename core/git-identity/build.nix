{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "git-identity"; version = "0.1.0"; src = ./.;
  buildPhase = '' cc ${builtins.toString cflags} -I${./..} git_identity.c ${./../lib}/safe-exec.c -o git-identity '';
  installPhase = '' mkdir -p $out/bin; cp git-identity $out/bin/ '';
}
