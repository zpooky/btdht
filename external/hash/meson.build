sphash_src = files([
  'sha1.cpp'
])

sphash_include = include_directories('.')

sphash_lib = static_library('sphash',
              sphash_src,
              include_directories: sphash_include
              )

sphash_dep = declare_dependency(
              include_directories: sphash_include,
              link_with: sphash_lib
              )
