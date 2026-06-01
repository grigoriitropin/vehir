{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ], db, pain }:
pkgs.stdenv.mkDerivation {
  pname = "essentials-daemon"; version = "0.1.0"; src = ./.;
  buildInputs = [ pkgs.cjson pkgs.openssl pkgs.postgresql db pain ]; nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -I${./../lib} -I${db}/include -I${pain}/include \
      essentials_daemon_scaffold.c essentials_daemon_core.c ${./../lib}/vehir_lib.c \
      -o essentials-daemon -lcjson -lssl -lcrypto -lpq -L${db}/lib -ldb -L${pain}/lib -lpain
  '';
  installPhase = '' mkdir -p $out/bin; cp essentials-daemon $out/bin/ '';
}
