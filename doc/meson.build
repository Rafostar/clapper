gi_docgen = find_program('gi-docgen', required: get_option('doc'))
dot = find_program('dot', required: get_option('doc')) # Class hierarchy generation
build_doc = (gi_docgen.found() and dot.found() and get_option('doc'))

if build_doc
  if not build_gir
    error('Building documentation requires introspection to be compiled')
  endif
  subdir('reference')
endif
