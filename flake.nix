{
  # rekta-core and its demo, for anyone with Nix:
  #   nix run github:DerDaehne/Rekta-Design      build and start the demo
  #   nix build                                  result/bin/rekta-demo, result/lib/librekta.a, result/include/rekta.h
  #   nix develop                                a shell with the compiler, make, Wayland and X11 (Linux) and node for the tests
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
          buildFlags = [ "all" ];
          doCheck = true;
          nativeCheckInputs = [ pkgs.nodejs ];
          checkTarget = "test";
          installPhase = ''
            runHook preInstall
            install -Dm755 build/demo $out/bin/rekta-demo
            install -Dm644 build/librekta.a $out/lib/librekta.a
            install -Dm644 include/rekta.h $out/include/rekta.h
            install -Dm644 man/rekta.3 $out/share/man/man3/rekta.3
            runHook postInstall
          '';
          meta = {
            description = "Rekta Design as native views in C, and a demo of every feature";
            homepage = "https://github.com/DerDaehne/Rekta-Design";
            license = pkgs.lib.licenses.mit;
            mainProgram = "rekta-demo";
            platforms = systems;
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
            ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.pkg-config pkgs.gdb pkgs.xvfb-run pkgs.weston pkgs.mandoc ];
          shellHook = ''
            echo "rekta-core: make -C core && ./core/build/demo   (make -C core test, shots, sanitize)"
          '';
        };
      });

      checks = each (pkgs: { default = self.packages.${pkgs.stdenv.hostPlatform.system}.default; });

      formatter = each (pkgs: pkgs.nixfmt-rfc-style);
    };
}
