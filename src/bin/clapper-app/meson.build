clapperapp_option = get_option('clapper-app')
app_id = 'com.github.rafostar.Clapper'
app_resource_prefix = '/com/github/rafostar/Clapper/clapper-app'
build_clapperapp = false

if clapperapp_option.disabled()
  subdir_done()
endif

clapperapp_deps = [
  clapper_dep,
  clappergtk_dep,
  gst_dep,
  gtk4_dep,
  libadwaita_dep,
  glib_dep,
  gobject_dep,
]

foreach dep : clapperapp_deps
  if not dep.found()
    if clapperapp_option.enabled()
      error('clapper-app option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

subdir('data')
subdir('po')

clapperapp_resources = gnome.compile_resources(
  'clapper-app-resources',
  'clapper-app.gresources.xml',
  c_name: 'clapper_app',
)

# Include the generated headers
clapperapp_conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name() + '-app')
config_h.set_quoted('LOCALEDIR', join_paths (prefix, localedir))
config_h.set_quoted('CLAPPER_APP_NAME', 'Clapper')
config_h.set_quoted('CLAPPER_APP_ID', app_id)
config_h.set_quoted('CLAPPER_APP_RESOURCE_PREFIX', app_resource_prefix)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

clapperapp_sources = [
  'clapper-app-about-dialog.c',
  'clapper-app-application.c',
  'clapper-app-file-dialog.c',
  'clapper-app-headerbar.c',
  'clapper-app-info-window.c',
  'clapper-app-list-item-utils.c',
  'clapper-app-media-item-box.c',
  'clapper-app-preferences-window.c',
  'clapper-app-property-row.c',
  'clapper-app-queue-list.c',
  'clapper-app-queue-progression-item.c',
  'clapper-app-queue-progression-model.c',
  'clapper-app-queue-selection.c',
  'clapper-app-types.c',
  'clapper-app-uri-dialog.c',
  'clapper-app-utils.c',
  'clapper-app-window.c',
  'clapper-app-window-state-buttons.c',
  'main.c',
  clapperapp_resources,
]
clapperapp_c_args = [
  '-DG_LOG_DOMAIN="ClapperApp"',
  '-DGST_USE_UNSTABLE_API',
]

executable(
  meson.project_name(),
  clapperapp_sources,
  dependencies: clapperapp_deps,
  include_directories: clapperapp_conf_inc,
  c_args: clapperapp_c_args,
  install: true,
  install_dir: bindir,
  win_subsystem: 'windows',
)
if ['windows'].contains(host_machine.system())
  executable(
    meson.project_name() + '-console',
    clapperapp_sources,
    dependencies: clapperapp_deps,
    include_directories: clapperapp_conf_inc,
    c_args: clapperapp_c_args,
    install: true,
    install_dir: bindir,
    win_subsystem: 'console',
  )
endif
build_clapperapp = true
