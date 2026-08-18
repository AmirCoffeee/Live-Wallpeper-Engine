project('live-wallpaper', 'c',
        version : '2.0.0',
        default_options : ['warning_level=1', 'c_std=c11']
)

gtk3_dep   = dependency('gtk+-3.0')
gst_dep    = dependency('gstreamer-1.0')
gstvid_dep = dependency('gstreamer-video-1.0')
x11_dep    = dependency('x11')
xext_dep   = dependency('xext')

sources = files(
  'src/main.c',
  'src/wallpaper.c',
  'src/gallery.c',
  'src/gui.c',
  'src/config.c',
)

executable('live-wallpaper',
           sources,
           dependencies : [gtk3_dep, gst_dep, gstvid_dep, x11_dep, xext_dep],
           install : true,
           install_dir : get_option('bindir')
)

# Install .desktop file
install_data('live-wallpaper.desktop',
    install_dir : join_paths(get_option('datadir'), 'applications')
)
