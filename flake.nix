{
  description = "A small program to toggle numlock across all of Wayland";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "numlockwl";
        version = "0.1";
        src = ./.;

        buildInputs = [ pkgs.libevdev ];

        nativeBuildInputs = [ pkgs.pkg-config ];

        # Override compiler flags if needed
        makeFlags = [ "CC=${pkgs.gcc}/bin/gcc" ];

        installPhase = ''
          mkdir -p $out/bin
          cp numlockwl $out/bin/
        '';
      };

      devShells.${system}.default = pkgs.mkShell {
        buildInputs = [ pkgs.gcc pkgs.libevdev pkgs.pkg-config ];
      };
    };
}
