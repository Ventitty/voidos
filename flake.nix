{
  description = "FHS Shell universel pour dev embarqué (ESP32-S3)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = let
        fhs = pkgs.buildFHSEnv {
          name = "esp-fhs-env";
          targetPkgs = p: with p; [
            gnumake
            wget
            xz
            qemu
            zlib
            glibc
            python3
            ncurses5
            pixman
            glib
            libgcrypt
            SDL2
            libslirp
            gdb

            esptool
            picocom
          ];
          runScript = "bash";
        };
      in fhs.env;
    };
}
