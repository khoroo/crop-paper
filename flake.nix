{
  description = "cropper - crop images to a given aspect ratio";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    systems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = nixpkgs.lib.genAttrs systems;

    perSystem = f: forAllSystems (system: let
      pkgs = import nixpkgs { inherit system; };

      my-font = pkgs.jetbrains-mono;

      fontsConf = pkgs.makeFontsConf {
        fontDirectories = [ my-font ];
      };
    in f pkgs my-font fontsConf);
  in {
    packages = perSystem (pkgs: my-font: fontsConf: {
      default = pkgs.stdenv.mkDerivation {
        pname = "cropper";
        version = "0.1.0";
        src = nixpkgs.lib.cleanSource ./.;

        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildInputs = with pkgs; [ raylib fontconfig my-font ];

        env.NIX_CFLAGS_COMPILE = "-std=c23 -Wall -Wextra -pedantic -O2";

        buildPhase = ''
          export FONTCONFIG_FILE="${fontsConf}"
          $CC \
            $(pkg-config --cflags raylib) \
            $(pkg-config --libs raylib) -lm \
            $(pkg-config --cflags --libs fontconfig) \
            -o cropper src/cropper.c
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp cropper $out/bin/
        '';

        meta = with nixpkgs.lib; {
          description = "Crop images to a given aspect ratio";
          license = licenses.mit;
          platforms = platforms.linux;
          maintainers = [ ];
        };
      };
    });

    devShells = perSystem (pkgs: my-font: fontsConf: {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [ raylib fontconfig my-font pkg-config ];

        shellHook = ''
          export FONTCONFIG_FILE="${fontsConf}"
        '';
      };
    });

    formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixpkgs-fmt);
  };
}
