config = debug
system = linux
target = server

all: build/$(system)/$(config)/rules.ninja
	@cmake --build build/$(system)/$(config) --target $(target)

run: run/$(system)

run/windows: build/windows/$(config)/rules.ninja
	@cmake --build build/windows/$(config) --target $(target)
	@build\windows\$(config)\$(target).exe

run/linux: build/linux/$(config)/rules.ninja
	@cmake --build build/linux/$(config) --target $(target)
	@build/linux/$(config)/$(target)

install: build/$(system)/release/rules.ninja
	@cmake --build build/$(system)/release --target install

package: build/$(system)/release/rules.ninja
	@cmake --build build/$(system)/release --target package

node_modules:
	@npm install

watch: node_modules
	@npm run watch

clean:
	@cmake -E remove_directory build/html
	@cmake -E remove_directory build/$(system)

build/windows/debug/rules.ninja: CMakeLists.txt
	@cmake -GNinja -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX="$(MAKEDIR)\build\install" \
	  -DCMAKE_TOOLCHAIN_FILE="$(MAKEDIR)\res\toolchain.cmake" \
	  -B build/windows/debug

build/windows/release/rules.ninja: CMakeLists.txt
	@cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX="$(MAKEDIR)\build\install" \
	  -DCMAKE_TOOLCHAIN_FILE="$(MAKEDIR)\res\toolchain.cmake" \
	  -B build/windows/release

build/linux/debug/rules.ninja: CMakeLists.txt
	@cmake -GNinja -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX="$(CURDIR)/build/install" \
	  -DCMAKE_TOOLCHAIN_FILE="$(CURDIR)/res/toolchain.cmake" \
	  -B build/linux/debug

build/linux/release/rules.ninja: CMakeLists.txt
	@cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX="$(CURDIR)/build/install" \
	  -DCMAKE_TOOLCHAIN_FILE="$(CURDIR)/res/toolchain.cmake" \
	  -B build/linux/release
