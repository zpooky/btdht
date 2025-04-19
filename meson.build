project('spdht', 'cpp',
        version: '1.0.0',
        license: 'GPL',
        default_options : ['warning_level=3', 'cpp_std=c++20', 'debug=true'])

subdir('external')

spdht_includes = [
  include_directories('src'),
  sphash_include,
]

cpp = meson.get_compiler('cpp')
m_dep = cpp.find_library('m')
miniupnpc_dep = cpp.find_library('miniupnpc')
sqlite3_dep = cpp.find_library('sqlite3')

spdht_deps = [
  sputil_dep,
  sphash_dep,
  m_dep,
  miniupnpc_dep,
  sqlite3_dep,
]

subdir('src')

spdht_lib = static_library('spdht',
              spdht_src,
              include_directories: spdht_includes,
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
          dependencies: spdht_deps
          )

executable('dht-server',
          'dht-server.cpp',
          dependencies: spdht_deps
          )

# add_project_arguments('-DNDEBUG', language : ['cpp'])
