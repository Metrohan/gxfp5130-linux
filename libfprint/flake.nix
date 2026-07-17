{
  description = "gxfp fprintd packages for NixOS";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-linux"];
    forEachSystem = f:
      nixpkgs.lib.genAttrs supportedSystems (system:
        f (import nixpkgs {
          inherit system;
        }));
  in {
    overlays.default = final: _prev: {
      gxfp-tools = final.callPackage ./nix/gxfp-tools.nix {};
    };

    packages = forEachSystem (pkgs: rec {
      libfprint-gxfp = pkgs.callPackage ./nix/libfprint.nix {};
      fprintd-gxfp = pkgs.callPackage ./nix/fprintd.nix {
        inherit libfprint-gxfp;
      };
      gxfp-tools = pkgs.callPackage ./nix/gxfp-tools.nix {};
      default = fprintd-gxfp;
    });

    nixosModules = rec {
      fprintd-gxfp = import ./nix/module.nix self;
      default = fprintd-gxfp;
    };
  };
}
