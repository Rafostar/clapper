clapper_toml = configure_file(
  input: 'clapper.toml.in',
  output: 'clapper.toml',
  configuration: doc_version_conf,
  install: true,
  install_dir: join_paths(datadir, 'doc', 'clapper'),
)

extra_md_files = [
  'clapper-enhancers.md',
  'extractable-enhancers.md',
  'playlistable-enhancers.md',
  'reactable-enhancers.md',
  'migrating-to-010.md',
]

custom_target('clapper-doc',
  input: [
    clapper_toml,
    clapper_gir[0],
  ],
  output: 'clapper',
  command: [
    gi_docgen,
    'generate',
    gi_docgen_common_args,
    '--add-include-path=@0@'.format(join_paths(meson.project_build_root(), 'src', 'lib', 'clapper')),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_build_dir()),
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depend_files: extra_md_files,
  build_by_default: true,
  install: true,
  install_dir: join_paths(datadir, 'doc'),
)
