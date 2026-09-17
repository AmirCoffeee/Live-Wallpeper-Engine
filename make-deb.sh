#!/usr/bin/env bash
set -e

PKG="live-wallpaper"
VER="2.0.0"
ARCH="amd64"
DEB_NAME="${PKG}_${VER}_${ARCH}.deb"

echo ">>> Building binary..."
rm -rf build
meson setup build --buildtype=release --prefix=/usr
ninja -C build

echo ">>> Creating package structure..."
PKGDIR="$(mktemp -d)/pkg"
mkdir -p "$PKGDIR/usr/bin"
mkdir -p "$PKGDIR/usr/share/applications"
mkdir -p "$PKGDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$PKGDIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$PKGDIR/DEBIAN"

# Binary
cp build/live-wallpaper "$PKGDIR/usr/bin/live-wallpaper"
chmod 755 "$PKGDIR/usr/bin/live-wallpaper"

# .desktop file
cp live-wallpaper.desktop "$PKGDIR/usr/share/applications/live-wallpaper.desktop"

# DEBIAN/control
cat > "$PKGDIR/DEBIAN/control" << EOF
Package: live-wallpaper
Version: ${VER}
Architecture: ${ARCH}
Maintainer: Live Wallpaper Engine <noreply@localhost>
Depends: libgtk-3-0, libgstreamer1.0-0, libgstreamer-plugins-base1.0-0, gstreamer1.0-plugins-good, gstreamer1.0-x, ffmpeg
Section: utils
Priority: optional
Description: Live Video Wallpaper Engine for GNOME
 Set any video file as your live desktop wallpaper.
 Runs in the system tray, applies on login automatically.
EOF

# DEBIAN/postinst — update icon cache after install
cat > "$PKGDIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /usr/share/applications || true
fi
EOF
chmod 755 "$PKGDIR/DEBIAN/postinst"

# DEBIAN/postrm — clean up icon cache on remove
cat > "$PKGDIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "$PKGDIR/DEBIAN/postrm"

echo ">>> Building .deb..."
fakeroot dpkg-deb --build "$PKGDIR" "$DEB_NAME"

echo ""
echo "✓ Done: $DEB_NAME"
echo ""
echo "Install with:"
echo "  sudo dpkg -i $DEB_NAME"
echo ""
echo "After install, enable Start on Login from the tray Settings menu."
