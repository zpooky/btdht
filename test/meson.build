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

spdht_test = executable('thetest',
                      spdht_test_src,
                      include_directories: spdht_includes,
                      link_args: spdht_link_options,
                      cpp_args: spdht_compile_options,
                      dependencies: spdht_test_deps
                     )

test('spdht test', spdht_test)
