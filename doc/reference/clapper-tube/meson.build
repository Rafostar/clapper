clappertube_toml = configure_file(
  input: 'clapper-tube.toml.in',
  output: 'clapper-tube.toml',
  configuration: doc_version_conf,
  install: true,
  install_dir: join_paths(datadir, 'doc', 'clapper-tube'),
)

custom_target('clapper-tube-doc',
  input: [
    clappertube_toml,
    clappertube_gir[0],
  ],
  output: 'clapper-tube',
  command: [
    gi_docgen,
    'generate',
    gi_docgen_common_args,
    '--add-include-path=@0@'.format(join_paths(meson.project_build_root(), 'src', 'lib', 'clapper')),
    '--add-include-path=@0@'.format(join_paths(meson.project_build_root(), 'src', 'lib', 'clapper-tube')),
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
