self: {
  lib,
  config,
  pkgs,
  ...
}: let
  cfg = config.hardware.gxfp;
in {
  config = lib.mkIf (cfg.enable) {
    services.udev.extraRules = ''
      SUBSYSTEM=="misc", KERNEL=="gxfp", MODE="0660", TAG+="uaccess"
    '';

    systemd.services.fprintd.serviceConfig.DeviceAllow = lib.mkAfter [
      "/dev/gxfp rw"
    ];

    services.fprintd = {
      enable = true;
      package = lib.mkForce self.packages.${pkgs.system}.fprintd-gxfp;
    };
  };
}
