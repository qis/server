# Server
C++ server example.

## Requirements
Install Visual Studio 2019 on Windows and Clang >= 9 or GCC >= 9 on Linux.

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt install build-essential binutils-dev gcc-9 g++-9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
```

Use [vcpkg](https://github.com/microsoft/vcpkg) to install dependencies.

```sh
vcpkg install boost boost-json date fmt spdlog
```

## Usage
1. Install and update `npm` dependencies.

```sh
npm install
npm install @babel/core@latest --save-dev
npm install autoprefixer@latest --save-dev
npm install browserslist@latest --save-dev
npm install parcel-bundler@latest --save-dev
npm install parcel-plugin-svelte@latest --save-dev
npm install postcss-modules@latest --save-dev
npm install svelte@latest --save-dev
```

2. Verify and adjust browserlist in [package.json](package.json).

```sh
npx browserslist
```

3. Open the directory as a CMake project in Visual Studio or use [makefile](makefile) commands.

* `make` to build (debug)
* `make run` to build and run (debug)
* `make watch` to watch the html source directory
* `make install` to build and install into `build/install` (release)
* `make format` to format code with [clang-format](https://llvm.org/builds/)
* `make clean` to remove build files

Add `config=release` to switch from debug to release mode.
