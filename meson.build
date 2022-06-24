project('com.github.rafostar.Clapper', 'c', 'cpp',
  version: '0.5.2',
  meson_version: '>= 0.50.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized'
  ]
)

gnome = import('gnome')
i18n = import('i18n')
python = import('python')

bindir = join_paths(get_option('prefix'), get_option('bindir'))
libdir = join_paths(get_option('prefix'), get_option('libdir'))
datadir = join_paths(get_option('prefix'), get_option('datadir'))

pkglibdir = join_paths(libdir, meson.project_name())
pkgdatadir = join_paths(datadir, meson.project_name())

subdir('lib')

if get_option('player')
  subdir('bin')
  subdir('data')
  subdir('po')

  install_subdir('src', install_dir: pkgdatadir)
  install_subdir('extras', install_dir: pkgdatadir)
  install_subdir('css', install_dir: pkgdatadir)
  install_subdir('ui', install_dir: pkgdatadir)

  python_bin = python.find_installation('python3')
  if not python_bin.found()
    error('No valid python3 binary found')
  endif
  meson.add_install_script('build-aux/meson/postinstall.py')
endif
