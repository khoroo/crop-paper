{
  description = "cropper - crop images to a given aspect ratio";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    system = "aarch64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in {
    devShells.${system}.default = pkgs.mkShell {
      buildInputs = with pkgs; [
        raylib
        dejavu_fonts
        pkg-config
      ];
      shellHook = ''
        cat > font_path.h <<EOF
#define FONT_PATH "${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSansMono.ttf"
EOF
      '';
    };

    packages.${system}.default = pkgs.stdenv.mkDerivation {
      pname = "cropper";
      version = "0.1.0";
      src = ./.;
      buildInputs = with pkgs; [ raylib dejavu_fonts ];
      nativeBuildInputs = with pkgs; [ pkg-config ];
      buildPhase = ''
        cat > font_path.h <<EOF
#define FONT_PATH "${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSansMono.ttf"
EOF
        make
      '';
      installPhase = ''
        mkdir -p $out/bin
        cp cropper $out/bin/
      '';
    };
  };
}
