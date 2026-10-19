# Neden app icon

`neden.png` is the production master of the user's selected A2 icon: an inset white lowercase n on an azure rounded-square tile. It was generated with the built-in Image Generation tool from the selected contact-sheet reference; the prompt preserved A2's geometry and requested transparent space outside the tile.

The desktop app embeds this PNG for its welcome screen and window icon. `apps/desktop/macos/Neden.icns` contains the same image at macOS's 16, 32, 128, 256, and 512 pt sizes, each at 1× and 2×. To update it, resize the master into the corresponding `Neden.iconset/icon_<size>x<size>[@2x].png` files using `sips`, then run `iconutil -c icns Neden.iconset -o apps/desktop/macos/Neden.icns`.

Exploration sheets are not runtime assets and are not stored in this repository.
