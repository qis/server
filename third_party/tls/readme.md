# LibreSSL
Version: 2.6.3

```sh
wget https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.6.3.tar.gz
tar xf libressl-2.6.3.tar.gz
```

## Windows
```cmd
md libressl-2.6.3\build && cd libressl-2.6.3\build
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_CONFIGURATION_TYPES="Debug;Release" ^
  -DCMAKE_INSTALL_PREFIX:PATH=../../libressl ..
cmake --build . --target install --config Release
cmake --build . --target install --config Debug
```

## Unix
```sh
mkdir libressl-2.6.3/build && cd libressl-2.6.3/build
CC=clang cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=../../libressl ..
cmake --build . --target install
```
