{
  description = "crop-paper - crop images to a given aspect ratio";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    systems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = nixpkgs.lib.genAttrs systems;

    perSystem = f: forAllSystems (system: let
      pkgs = import nixpkgs { inherit system; };
      my-font = pkgs.iosevka;
    in f pkgs my-font);
  in {
    packages = perSystem (pkgs: my-font: {
      default = pkgs.stdenv.mkDerivation {
        pname = "crop-paper";
        version = "0.1.0";
        src = nixpkgs.lib.cleanSource ./.;

        nativeBuildInputs = with pkgs; [ pkg-config python3Packages.fonttools ];
        buildInputs = with pkgs; [ raylib my-font ];

        env.NIX_CFLAGS_COMPILE = "-std=c23 -Wall -Wextra -pedantic -O2";

        buildPhase = ''
          pyftsubset ${my-font}/share/fonts/truetype/Iosevka-Medium.ttf \
            --unicodes="U+0020-007E" \
            --output-file=iosevka-subset.ttf

          python -c "
          import sys
          data = open('iosevka-subset.ttf', 'rb').read()
          print('unsigned char iosevka_subset_ttf[] = {')
          for i in range(0, len(data), 12):
              print('  ' + ', '.join(f'0x{b:02x}' for b in data[i:i+12]) + ',')
          print('};')
          print(f'unsigned int iosevka_subset_ttf_len = {len(data)};')
          " > font-embed.h

          $CC \
            $(pkg-config --cflags raylib) \
            $(pkg-config --libs raylib) -lm \
            -I. \
            -o crop-paper src/crop-paper.c
        '';

        installPhase = ''
          mkdir -p $out/bin
          cp crop-paper $out/bin/
        '';

        meta = with nixpkgs.lib; {
          description = "Crop images to a given aspect ratio";
          license = licenses.mit;
          platforms = platforms.linux;
          maintainers = [ ];
        };
      };
    });

    devShells = perSystem (pkgs: my-font: {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [ raylib my-font pkg-config python3Packages.fonttools ];
      };
    });

    formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixpkgs-fmt);
  };
}
