{ pkgs, cflags ? [ "-Wall" "-Wextra" "-Werror" "-std=c2x" ] }:
pkgs.stdenv.mkDerivation {
  pname = "vehir-config";
  version = "0.1.0";
  src = ./.;
  buildInputs = [ pkgs.cjson ];
  nativeBuildInputs = [ pkgs.pkg-config ];
  buildPhase = ''
    cc ${builtins.toString cflags} -c broker.c -o broker.o
    ar rcs libvehir-config.a broker.o
    cc ${builtins.toString cflags} broker.c -o broker -lcjson
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib $out/include
    cp broker $out/bin/
    cp libvehir-config.a $out/lib/
    cp broker.h $out/include/
  '';
}
