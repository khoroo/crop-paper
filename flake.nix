{
  description = "cropper - crop images to a given aspect ratio";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    systems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = nixpkgs.lib.genAttrs systems;
  in {
    packages = forAllSystems (system: let
      pkgs = import nixpkgs { inherit system; };
      fontPath = "${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSansMono.ttf";
    in {
      default = pkgs.stdenv.mkDerivation {
        pname = "cropper";
        version = "0.1.0";
        src = nixpkgs.lib.cleanSource ./.;

        nativeBuildInputs = with pkgs; [ pkg-config ];
        buildInputs = with pkgs; [ raylib dejavu_fonts ];

        env.NIX_CFLAGS_COMPILE = "-std=c23 -Wall -Wextra -pedantic -O2";

        preBuild = ''
          cat > font_path.h <<EOF
#define FONT_PATH "${fontPath}"
EOF
        '';

        buildPhase = ''
          $CC \
            $(pkg-config --cflags raylib) \
            $(pkg-config --libs raylib) -lm \
            -include font_path.h \
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

    devShells = forAllSystems (system: let
      pkgs = import nixpkgs { inherit system; };
    in {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [ raylib dejavu_fonts pkg-config ];
      };
    });

    formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixpkgs-fmt);

    checks = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.runCommand "cropper-check" { nativeBuildInputs = [ pkgs.gnugrep ]; } ''
        ${self.packages.${system}.default}/bin/cropper 2>&1 \
          | grep -q "Usage:" || {
          echo "FAIL: expected usage message"
          exit 1
        }
        touch $out
      '';
    });
  };
}
