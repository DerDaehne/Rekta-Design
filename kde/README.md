# Rekta for KDE Plasma 6

The whole desktop in Rekta Design, built from `tokens/rekta.tokens.json` by
`scripts/build-kde.mjs` like every other platform. Two global themes,
**Rekta Light** and **Rekta Dark**, each set:

| Part | What it is | Installed to |
|---|---|---|
| Colour scheme | `RektaLight`, `RektaDark`: the ground, the ink, hairlines, the status colours. Selection is a grey (`text-contrast-low`) under the ink. GTK apps follow it. | `share/color-schemes/` |
| Plasma Style | `rekta`: flush panels with a hairline towards the desktop, popups on the ground with a 1px edge and radius-s and no shadow, task buttons with a 2px line on the screen-edge side, 2px ink buttons, the 6px slider with an ink bar, Metro switches, pivot-like tabs. It takes its colours from the colour scheme. | `share/plasma/desktoptheme/` |
| Window decoration | Aurorae `RektaLight`, `RektaDark`: rekta-core's title bar, 36px on the ground with a hairline below, the title on the left, 46px buttons with the icons of the set; a 1px edge, medium while active. | `share/aurorae/themes/` |
| Application style | Kvantum `Rekta` with `RektaDark`: Qt and KDE apps with 2px ink buttons, inputs with a 1px edge and a focus ring, checkboxes filled in ink, tabs underlined in ink, rows that take the hover overlay, menus with a check and no box. | `share/Kvantum/Rekta/` |
| Splash | The mark of the app icon: its line fills as Plasma starts, the success square appears when it is ready. | in the global themes |
| Wallpaper | `Rekta`: one straight line across the ground and the small square over its end, light and dark. | `share/wallpapers/` |
| Login screen | SDDM `rekta`: the date, the time as a large light title, the people as a pivot, one field, the session and power actions in the app bar. Barlow ships with it. | `share/sddm/themes/` |

## Build

```sh
nix build .#rekta-kde                    # result/share/…
node scripts/build-kde.mjs [prefix]      # or without Nix: [prefix]/share, default dist/kde (needs rsvg-convert)
```

## Install with Nix

The flake's `packages.<system>.rekta-kde` is a plain package: put it where
Plasma looks (`share/` of a profile) and pick the theme.

```nix
# flake inputs
rekta-design.url = "github:DerDaehne/Rekta-Design";

# NixOS: the login screen
environment.systemPackages = [ inputs.rekta-design.packages.${pkgs.system}.rekta-kde ];
services.displayManager.sddm.theme = "rekta";
```

```nix
# home-manager (with plasma-manager)
{ inputs, pkgs, ... }:
let rekta = inputs.rekta-design.packages.${pkgs.system}.rekta-kde; in
{
  home.packages = [ rekta pkgs.kdePackages.qtstyleplugin-kvantum ];

  # Kvantum reads themes only from its own folders, not from XDG_DATA_DIRS.
  xdg.configFile."Kvantum/Rekta".source = "${rekta}/share/Kvantum/Rekta";
  xdg.configFile."Kvantum/kvantum.kvconfig".text = "[General]\ntheme=Rekta\n";

  programs.plasma = {
    workspace.lookAndFeel = "design.rekta.dark";   # or design.rekta.light
    # 1px window edges: Aurorae raises the "Normal" border size to 4px.
    configFile.kwinrc."org.kde.kdecoration2" = { BorderSize = "Tiny"; BorderSizeAuto = false; };
  };
}
```

A global theme writes its choices as defaults. Settings of your own win over
them, so remove any you set before for the same things: `widgetStyle`, the
decoration's `library` and `theme`, `AccentColor` (Plasma's accent replaces
the scheme's selection and focus colours), the Plasma theme.

Without Nix, copy `share/` into `~/.local/share/`, link the Kvantum theme into
its folder (`ln -s ~/.local/share/Kvantum/Rekta ~/.config/Kvantum/`) and set
`theme=Rekta` in `~/.config/Kvantum/kvantum.kvconfig`.

The dark global theme sets the style `kvantum-dark`, the light one
`kvantum`: one Kvantum theme, Plasma picks its variant.
