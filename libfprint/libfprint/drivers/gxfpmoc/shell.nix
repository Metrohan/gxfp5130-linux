{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  buildInputs = [
    pkgs.cmake
    pkgs.gcc
    pkgs.pkg-config
    pkgs.mbedtls

    pkgs.clang-tools
    pkgs.bear
  ];
  shellHook = " ";
}
