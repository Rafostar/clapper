project('com.github.rafostar.Clapper',
  version: '0.0.0',
  meson_version: '>= 0.50.0',
  license: 'GPL3',
  default_options: [ 'warning_level=2' ]
)

python = import('python')
python_bin = python.find_installation('python3')

if not python_bin.found()
    error('No valid python3 binary found')
endif

conf = configuration_data()
conf.set('bindir', join_paths(get_option('prefix'), 'bin'))

subdir('bin')
subdir('gjs-1.0')
subdir('data')

installdir = join_paths(get_option('prefix'), 'share', meson.project_name())
install_subdir('clapper_src', install_dir : installdir)
install_subdir('css', install_dir : installdir)
install_subdir('ui', install_dir : installdir)

meson.add_install_script('build-aux/meson/postinstall.py')
