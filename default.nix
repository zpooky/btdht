with import <nixpkgs> {};

stdenv.mkDerivation rec {
  name = "project-name";

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    backward-cpp
    libbfd
    libunwind
    miniupnpc
    gtest
  ];

  buildInputs = [
  ];

  configureFlags = [
  ];
}
