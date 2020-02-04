# Server
C++ server example.

## Usage
1. Install dependencies and start watching sources in `bash.exe`.

```sh
make watch
```

2. Open the directory in VS Code.
3. Use the [makefile](makefile) on the command line.

```sh
make
make config=release
make run
make run config=release
make install
make package
make clean
```

## Requirements
This repository requires a working vcpkg setup with [custom toolchains](https://github.com/qis/toolchains).

For JSON serialization, use the [qis/boost-json](https://github.com/qis/boost-json) overlay.
