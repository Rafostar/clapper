# Same as "clapper-app-utils.c"
# FIXME: Have these in one place only
win_extensions = [
  'avi', 'claps', 'm2ts', 'mkv', 'mov',
  'mp4', 'webm', 'wmv'
]
reg_entries = []

foreach ext : win_extensions
  ext_id = 'Clapper.' + ext
  exe_path = '{app}\\bin\\clapper.exe'

  subkey = 'Software\\Classes\\.' + ext + '\\OpenWithProgids'
  reg_entries += 'Root: HKA; Subkey: "@0@"; ValueType: string; ValueName: "@1@"; ValueData: ""; Flags: uninsdeletevalue'.format(subkey, ext_id)

  subkey = 'Software\\Classes\\' + ext_id
  reg_entries += 'Root: HKA; Subkey: "@0@"; ValueType: string; ValueName: ""; ValueData: "@1@"; Flags: uninsdeletekey'.format(subkey, ext)

  subkey = 'Software\\Classes\\' + ext_id + '\\DefaultIcon'
  reg_entries += 'Root: HKA; Subkey: "@0@"; ValueType: string; ValueName: ""; ValueData: "@1@,0"'.format(subkey, exe_path)

  subkey = 'Software\\Classes\\' + ext_id + '\\shell\\open\\command'
  reg_entries += 'Root: HKA; Subkey: "@0@"; ValueType: string; ValueName: ""; ValueData: """@1@"" ""%1"""'.format(subkey, exe_path)

  reg_entries += '' # Empty line for clarity
endforeach

# NOTE: In meson replacing "@@" vars after "\" sign does not work.
# For this reason we do not pass app name here.
iss_conf = configuration_data()
iss_conf.set('CLAPPER_APP_VERSION', meson.project_version())
iss_conf.set('CLAPPER_APP_REG_ENTRIES', '\n'.join(reg_entries))
iss_conf.set('PROJECT_ROOT', meson.project_source_root())

# NOTE: We also need to package other dependencies, hence using whole prefix.
# Workaround to reduce package size is to set prefix to some custom dir, install,
# then determine the rest with "ldd" and copy remaining files manually there.
iss_conf.set('CLAPPER_APP_DEST', prefix.replace('/', '\\'))

configure_file(
  input: 'clapper.iss.in',
  output: 'clapper.iss',
  configuration: iss_conf,
)
