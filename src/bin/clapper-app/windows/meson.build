rc_conf = configuration_data()
rc_conf.set('VER_CLAPPERVERSION', clapper_version.replace('.', ',') + ',0')
rc_conf.set_quoted('PACKAGE', meson.project_name())
rc_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
rc_conf.set_quoted('PACKAGE_ORIGIN', 'https://github.com/Rafostar/clapper')
rc_conf.set_quoted('CLAPPER_APP_NAME', 'Clapper')
rc_conf.set_quoted('CLAPPER_EXE', meson.project_name() + '.exe')

clapperapp_rc = configure_file(
  input: 'clapper.rc.in',
  output: 'clapper.rc',
  configuration: rc_conf,
)

windres = find_program('windres', required: true)
clapperapp_sources += custom_target('clapper.o',
  input: clapperapp_rc,
  output: 'clapper.o',
  command: [windres, '-I', '@CURRENT_SOURCE_DIR@', '-o', '@OUTPUT@', '@INPUT@']
)
