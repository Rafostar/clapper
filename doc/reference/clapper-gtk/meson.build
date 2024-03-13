clappergtk_toml = configure_file(
  input: 'clapper-gtk.toml.in',
  output: 'clapper-gtk.toml',
  configuration: doc_version_conf,
  install: true,
  install_dir: join_paths(datadir, 'doc', 'clapper-gtk'),
)

custom_target('clapper-gtk-doc',
  input: [
    clappergtk_toml,
    clappergtk_gir[0],
  ],
  output: 'clapper-gtk',
  command: [
    gi_docgen,
    'generate',
    gi_docgen_common_args,
    '--add-include-path=@0@'.format(join_paths(meson.project_build_root(), 'src', 'lib', 'clapper-gtk')),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: join_paths(datadir, 'doc'),
)
