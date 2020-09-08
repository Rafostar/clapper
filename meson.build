project('com.github.rafostar.Clapper',
  version: '0.0.0',
  meson_version: '>= 0.50.0',
  default_options: [ 'warning_level=2' ]
)

python = import('python3')
python_bin = python.find_python()

if not python_bin.found()
    error('No valid python3 binary found')
endif

conf = configuration_data()
conf.set('bindir', join_paths([ get_option('prefix'), 'bin' ]))
conf.set('sharedir', join_paths([ get_option('prefix'), 'share' ]))
conf.set('gjsdir', join_paths([ get_option('sharedir'), 'gjs-1.0' ]))

srcdir = include_directories('clapper_src')
cssdir = include_directories('css')
subdir('bin')
subdir('gjs-1.0')

meson.add_install_script('build-aux/meson/postinstall.py')
