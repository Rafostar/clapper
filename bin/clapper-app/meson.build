clapperapp_option = get_option('clapperapp')
app_id = 'com.github.rafostar.Clapper'
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
      error('clapperapp option was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

subdir('data')

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

clapperapp_sources = [
  'clapper-app-application.c',
  'clapper-app-media-item-box.c',
  'clapper-app-queue-list.c',
  'clapper-app-queue-progression-item.c',
  'clapper-app-queue-progression-model.c',
  'clapper-app-headerbar.c',
  'clapper-app-file-dialog.c',
  'clapper-app-uri-dialog.c',
  'clapper-app-about-window.c',
  'clapper-app-window.c',
  'clapper-app-window-state-buttons.c',
  'clapper-app-utils.c',
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
)
build_clapperapp = true
