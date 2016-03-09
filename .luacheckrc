files['.luacheckrc'].global = false
files['spec'].std = 'busted+min'
files['*.rockspec'].globals = {
  'rockspec_format',
  'package',
  'version',
  'description',
  'supported_platforms',
  'dependencies',
  'external_dependencies',
  'source',
  'build',
}
