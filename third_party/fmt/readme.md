# {fmt}
Version: 4.0.0

```sh
wget https://github.com/fmtlib/fmt/archive/4.0.0.tar.gz
tar xf 4.0.0.tar.gz
```

## Windows
```cmd
md fmt-4.0.0\build && cd fmt-4.0.0\build
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_CONFIGURATION_TYPES="Debug;Release" ^
  -DFMT_DOC:BOOL=OFF -DFMT_TEST:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=../../fmt ..
cmake --build . --target install --config Release
cmake --build . --target install --config Debug
```

## Unix
```sh
mkdir fmt-4.0.0/build && cd fmt-4.0.0/build
CC=clang CXX=clang++ cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DFMT_DOC:BOOL=OFF -DFMT_TEST:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=../../fmt ..
cmake --build . --target install
```
