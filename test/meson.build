spdht_test_src = files([
  'krpcTest.cpp',
  'dumpTest.cpp',
  'bencodeTest.cpp',
  'dhtTest.cpp',
  'krpc2Test.cpp',
  'timout_test.cpp',
  'upnpTest.cpp',
  'transactionTest.cpp',
  'mainlineTest.cpp',
  'utilTest.cpp',
])

spdht_test_deps = spdht_deps
gtest_dep = dependency('gtest_main', required: false)

if gtest_dep.found()
  spdht_test_deps += gtest_dep

  spdht_test = executable('thetest',
                        spdht_test_src,
                        include_directories: spdht_includes,
                        dependencies: spdht_test_deps
                       )

  test('spdht test', spdht_test)
endif
