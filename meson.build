project('spdht', 'cpp',
        version: '1.0.0',
        license: 'GPL')

spdht_compile_options = [
  '-std=c++17',
  '-enable-frame-pointers',
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  '-Wpointer-arith',
  '-Wconversion',
  '-Wshadow'
]

spdht_compile_options += [
  '-ggdb',
  '-O0'
]

spdht_link_options = [
  '-fsanitize=address',
  '-fno-omit-frame-pointer'
]

subdir('external')

spdht_includes = [
  include_directories('src'),
  sphash_include,
]

cpp = meson.get_compiler('cpp')
m_dep = cpp.find_library('m')
miniupnpc_dep = cpp.find_library('miniupnpc')

spdht_deps = [
  sputil_dep,
  sphash_dep,
  m_dep,
  miniupnpc_dep,
]

subdir('src')

spdht_lib = static_library('spdht',
              spdht_src,
              include_directories: spdht_includes,
              cpp_args: spdht_compile_options,
              dependencies: spdht_deps
              )

spdht_dep = declare_dependency(
              include_directories: spdht_includes,
              link_with: spdht_lib,
              dependencies: spdht_deps
              )

spdht_deps += spdht_dep
subdir('test')

executable('dht-client',
          'dht-client.cpp',
          link_args: spdht_link_options,
          cpp_args: spdht_compile_options,
          dependencies: spdht_deps
          )

executable('dht-server',
          'dht-server.cpp',
          link_args: spdht_link_options,
          cpp_args: spdht_compile_options,
          dependencies: spdht_deps
          )

# add_project_arguments('-DNDEBUG', language : ['cpp'])
