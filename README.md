# Live Wallpaper Engine

Set any video file as your live desktop wallpaper on GNOME/Ubuntu.

## Features

- Play MP4, MKV, WebM, AVI and other video formats as your desktop wallpaper
- System tray icon — runs silently in the background
- Auto-applies the last wallpaper on startup
- Start on login support
- Gallery UI to manage and switch wallpapers
- Mute audio option
- Frame from video shown in Activities / App Drawer

## Requirements

- Ubuntu 22.04+ / GNOME 42+
- GStreamer 1.0 with good/bad plugins
- GTK 3.24+
- ffmpeg (for thumbnail extraction)
- X11 (Xorg session)

## Build from source

```bash
# Install dependencies
sudo apt install \
    meson ninja-build pkg-config \
    libgtk-3-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libx11-dev libxext-dev \
    ffmpeg

# Build
bash build.sh

# Run
./build/live-wallpaper
```

## Install via .deb

Build and install a `.deb` package:

```bash
bash make-deb.sh
sudo dpkg -i live-wallpaper_2.0.0_amd64.deb
```

After installing, launch **Live Wallpaper Engine** from your app drawer or run `live-wallpaper`.

## Usage

1. Click **+ Add** to add a video file to the gallery
2. Click a card to apply it as your wallpaper instantly
3. The app minimizes to the system tray
4. Right-click the tray icon to access Gallery, Settings, or Quit
5. In Settings, enable **Start on login** to auto-start with your session

## Project structure

```
src/
  main.c        — entry point, single-instance lock, signal handling
  wallpaper.c   — GStreamer pipeline, X11 window management, GNOME bg
  gallery.c     — gallery window, card grid, thumbnail rendering
  gui.c         — settings window, tray icon
  config.c      — load/save config (~/.config/live-wallpaper/config.json)
  *.h           — headers
meson.build     — build definition
build.sh        — quick build script
make-deb.sh     — builds a .deb package
icon.png        — application icon
live-wallpaper.desktop — desktop entry
```

## Notes

- Requires an **Xorg** session (Wayland is not supported)
- The `xapp-gtk3-module` warning on startup is harmless — it's a Linux Mint module not present on Ubuntu
- Thumbnails are cached in `~/.cache/live-wallpaper/thumbs/`

## License

MIT
