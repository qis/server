# Server
C++ server example.

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

3. Open the directory in VS Code and run the `watch` task or use the command line.

```sh
make watch
```

4. Use the default build task in VS Code or [makefile](makefile) on the command line.

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
