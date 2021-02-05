project('com.github.rafostar.Clapper', 'c', 'cpp',
  version: '0.0.0',
  meson_version: '>= 0.50.0',
  license: 'GPL3',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized'
  ]
)

python = import('python')
python_bin = python.find_installation('python3')

if not python_bin.found()
  error('No valid python3 binary found')
endif

conf = configuration_data()
conf.set('bindir', join_paths(get_option('prefix'), 'bin'))

if get_option('clapper-lib')
  subdir('lib')
endif

if get_option('clapper-player')
  subdir('bin')
  subdir('data')

  installdir = join_paths(get_option('prefix'), 'share', meson.project_name())
  install_subdir('src', install_dir : installdir)
  install_subdir('extras', install_dir : installdir)
  install_subdir('css', install_dir : installdir)
  install_subdir('ui', install_dir : installdir)

  meson.add_install_script('build-aux/meson/postinstall.py')
endif
