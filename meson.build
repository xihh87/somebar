project('somebar', ['c', 'cpp'],
  default_options: ['cpp_std=c++17'])

wayland_dep = dependency('wayland-client')
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Widgets'])
#moc = qt5.compile_moc(headers: [ 'src/.hpp', ])

subdir('protocols')

executable('somebar',
  'src/main.cpp',
  'src/shm_buffer.cpp',
  'src/bar.cpp',
  wayland_sources,
  #moc,
  dependencies: [qt5_dep, wayland_dep])
