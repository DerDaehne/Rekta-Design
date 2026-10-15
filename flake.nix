{
  # rekta-core and its demo, for anyone with Nix:
  #   nix run github:DerDaehne/Rekta-Design      build and start the demo
  #   nix build                                  result/bin/rekta-demo, result/lib/librekta.a, result/include/rekta.h
  #   nix develop                                a shell with the compiler, make, Wayland and X11 (Linux) and node for the tests
  #   nix build .#rekta-kde                      result/share: Rekta for KDE Plasma 6 (kde/README.md)
  #   nix flake check                            builds, and runs the tests and the reference images
  description = "Rekta Design: rekta-core, native views in C for Windows, macOS and Linux, and its demo";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];   # nixpkgs no longer builds for Intel Macs
      each = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
      # the system's windowing libraries are the only dependencies: Wayland (first) and Xlib on Linux; Cocoa comes with
      # the macOS SDK
      windowing = pkgs: pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [
        pkgs.wayland pkgs.libxkbcommon (pkgs.libx11 or pkgs.xorg.libX11) (pkgs.libxext or pkgs.xorg.libXext)
      ];
    in
    {
      packages = each (pkgs: rec {
        rekta-core = pkgs.stdenv.mkDerivation {
          pname = "rekta-core";
          version = "0.3.0";
          src = ./core;
          buildInputs = windowing pkgs;
          nativeBuildInputs = [ pkgs.pkg-config ];
          enableParallelBuilding = true;
          buildFlags = [ "all" "shared" ];
          doCheck = true;
          nativeCheckInputs = [ pkgs.nodejs ];
          checkTarget = "test";
          installPhase = ''
            runHook preInstall
            make install PREFIX=$out
            install -Dm755 build/demo $out/bin/rekta-demo
            runHook postInstall
          '';
          # fontconfig (for the characters Barlow lacks) and libdbus (the file chooser, notifications) are opened at run
          # time; Nix keeps them off the default path
          postFixup = pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
            for f in $out/bin/rekta-demo $out/lib/librekta.so.*.*.*; do
              patchelf --add-rpath ${pkgs.lib.getLib pkgs.fontconfig}/lib:${pkgs.lib.getLib pkgs.dbus}/lib "$f"
            done
          '';
          meta = {
            description = "Rekta Design as native views in C, and a demo of every feature";
            homepage = "https://github.com/DerDaehne/Rekta-Design";
            license = pkgs.lib.licenses.mit;
            mainProgram = "rekta-demo";
            platforms = systems;
          };
        };
        # Rekta for KDE Plasma 6, built from the tokens: global themes, colour schemes, Plasma Style, title bar, Kvantum
        # style, wallpaper and login screen. See kde/README.md.
        rekta-kde = pkgs.stdenvNoCC.mkDerivation {
          pname = "rekta-kde";
          version = (builtins.fromJSON (builtins.readFile ./package.json)).version;
          src = pkgs.lib.fileset.toSource {
            root = ./.;
            fileset = pkgs.lib.fileset.unions [ ./package.json ./tokens ./scripts ./src/icons ./kde ./core/fonts ];
          };
          nativeBuildInputs = [ pkgs.nodejs pkgs.librsvg ];
          dontConfigure = true;
          buildPhase = ''
            runHook preBuild
            node scripts/build-kde.mjs $out
            runHook postBuild
          '';
          dontInstall = true;
          meta = {
            description = "Rekta Design for KDE Plasma 6: global themes, Plasma Style, title bar, Kvantum style, wallpaper, login screen";
            homepage = "https://github.com/DerDaehne/Rekta-Design";
            license = pkgs.lib.licenses.mit;
            platforms = pkgs.lib.platforms.linux;
          };
        };
        default = rekta-core;
      });

      apps = each (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.default}/bin/rekta-demo";
          meta.description = "The rekta-core demo: a panorama of tiles, motion, controls, type and settings";
        };
      });

      devShells = each (pkgs: {
        default = pkgs.mkShell {
          packages = [ pkgs.gnumake pkgs.nodejs ] ++ windowing pkgs
            ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.pkg-config pkgs.gdb pkgs.xvfb-run pkgs.weston pkgs.mandoc pkgs.librsvg ];
          shellHook = ''
            echo "rekta-core: make -C core && ./core/build/demo   (make -C core test, shots, sanitize)"
          '';
        };
      });

      checks = each (pkgs: { default = self.packages.${pkgs.stdenv.hostPlatform.system}.default; }
        // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux { rekta-kde = self.packages.${pkgs.stdenv.hostPlatform.system}.rekta-kde; });

      formatter = each (pkgs: pkgs.nixfmt-rfc-style);
    };
}
