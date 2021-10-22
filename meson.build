project('somebar', ['c', 'cpp'],
  default_options: ['cpp_std=c++17'])

wayland_dep = dependency('wayland-client')
wayland_cursor_dep = dependency('wayland-cursor')
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])

subdir('protocols')

executable('somebar',
  'src/main.cpp',
  'src/shm_buffer.cpp',
  'src/bar.cpp',
  wayland_sources,
  dependencies: [qt5_dep, wayland_dep, wayland_cursor_dep])
