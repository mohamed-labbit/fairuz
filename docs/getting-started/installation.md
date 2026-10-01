# Build or install Fairuz

Fairuz requires CMake 3.14+, a C++23 compiler, zlib development files, and simdutf. CMake may fetch simdutf and GoogleTest when they are not cached. In the repository root:

```sh
./build.sh
./build/fairuz examples/hello.fa
```

For an installed copy, configure the prefix **before** building, so the recorded standard-library search path is correct:

```sh
cmake -S . -B build-install -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/fairuz
cmake --build build-install --target fairuz -j4
cmake --install build-install
/tmp/fairuz/bin/fairuz examples/hello.fa
```

Set `FAIRUZ_STDLIB` to the installed `share/fairuz/stdlib` directory if the install is relocated. The [running programs](running-programs.md) page explains CLI options. The current executable does not provide a REPL.
