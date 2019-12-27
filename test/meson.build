spdht_test_src = files([
  'krpcTest.cpp',
  'dumpTest.cpp',
  'bencodeTest.cpp',
  'cacheTest.cpp',
  'dhtTest.cpp',
  'krpc2Test.cpp',
  'timout_test.cpp',
  'upnpTest.cpp',
  'transactionTest.cpp',
  'mainlineTest.cpp'
])

spdht_test_deps = spdht_deps
spdht_test_deps += dependency('gtest_main')

spdht_test = executable('spdhttest',
                     spdht_test_src,
                     # include_directories: spdht_includes,
                     # link_with: spdht_lib,
                     cpp_args: spdht_compile_options,
                     dependencies: spdht_test_deps
                     )

test('spdht test', spdht_test)
