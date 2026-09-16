# Dogehiro

<img src="res/chihiro-doge.png" width="420">

A card editor for **Ghost Squad** (Sega Chihiro), for cards used with
[xemu](https://xemu.app) and its Chihiro card reader.

Open a card, change what the game would have let you earn (level, rank,
costumes, weapons, games left) and save it back. The game accepts the result:
Dogehiro writes the same bytes the game writes, seal and mirror included.

It also offers the fourteen items the game keeps but never lists: three costumes
and eleven weapons that have a model and a name of their own (`SAN92`, `CAC80`,
`SAW25`, `GGL50`, five kinds of grenade) with no card in the locker to reach
them. Written onto a card, the game wears them.

| Costumes | Weapons |
|---|---|
| ![](docs/costumes.png) | ![](docs/weapons.png) |

- **Open card**: pick a card file, 2048 bytes
- **Blank card**: a new card, as the reader issues one, with 100 games, one weapon and two costumes
- **Save card**: write the card back, sealed and mirrored, ready to use
- **Import assets**: read the game's pictures out of your own copy of the game
- **100%**: how large the page is drawn; your choice is remembered

In the two lists, left click wears an item and right click locks or unlocks it.

The imported pictures land in an `assets` folder beside the program, in the
World or the Japanese edition, your choice. It takes about a fifth of a second:
a Chihiro game image is an FATX filesystem, so the archives are opened by name
rather than hunted for. Without them the editor works the same, with names
instead of pictures.

To open a card without going through the dialog, name it when you start:

    dogehiro path/to/card.bin

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The window is the system's own web engine, so there is nothing to download:

| System  | Needs |
|---------|-------|
| Windows | the WebView2 runtime, already installed with Edge |
| macOS   | nothing, WKWebView is part of the system |
| Linux   | `libgtk-3-dev` and `libwebkit2gtk-4.1-dev` to build, the matching runtime packages to run |

On Linux one binary serves both display servers. GTK picks one when it starts,
and `GDK_BACKEND=x11` forces the older one. The window carries the doge under X11
but not under Wayland, which takes a window's icon from an installed `.desktop`
file rather than from the program itself.

## Testing

```
ctest --test-dir build
./build/dogehiro-selftest <card.bin> <game image>
```

The self-test opens a real card, checks that the level and rank computed from
EXP match what the game wrote, that saving touches only the write time and the
two checksums, that a worn item can never be locked, and that the textures
decoded from a game image come out byte for byte identical to the reference
extraction.

## Licence

GPL v3 or later, see `LICENSE`. Anything built on this stays open.

Two things are carried in `third_party` under their own terms, both permissive
and both compatible: [webview](https://github.com/webview/webview) (MIT),
amalgamated into one header, and the Microsoft WebView2 headers, used on Windows
only.

Every picture of the game belongs to Sega, and none of it is in the program you
download or in this repository. The two screenshots show the game's art because
that is what a screenshot is, and the logo at the top is the Shiba our own doge
mode paints over SEGABOOT.
