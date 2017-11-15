MAKEFLAGS += --no-print-directory

CC	!= which clang-devel || which clang
CXX	!= which clang++-devel || which clang++
CMAKE	:= CC=$(CC) CXX=$(CXX) cmake -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON
BUILD	!= echo $(PWD) | tr '/' '-' | sed 's|^-|/var/build/|'
PROJECT	!= grep "^project" CMakeLists.txt | cut -c9- | cut -d " " -f1 | tr "[:upper:]" "[:lower:]"
HEADERS	!= find src include -type f -name '*.h'
SOURCES	!= find src -type f -name '*.cpp'

all: debug

run: debug
	@sudo build/llvm/debug/server

dbg: debug
	@#lldb -o run build/llvm/debug/server
	@gdb -x run build/llvm/debug/server

test: debug
	@cd build/llvm/debug && ctest

tidy:
	@clang-tidy -p build/llvm/debug $(SOURCES) -header-filter=src

format:
	@clang-format -i $(HEADERS) $(SOURCES)

install: release
	@cmake --build build/llvm/release --target install

debug: build/llvm/debug/CMakeCache.txt $(HEADERS) $(SOURCES)
	@cmake --build build/llvm/debug

release: build/llvm/release/CMakeCache.txt $(HEADERS) $(SOURCES)
	@cmake --build build/llvm/release

build/llvm/debug/CMakeCache.txt: CMakeLists.txt build/llvm/debug
	@cd build/llvm/debug && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_INSTALL_PREFIX:PATH=$(PWD) $(PWD)

build/llvm/release/CMakeCache.txt: CMakeLists.txt build/llvm/release
	@cd build/llvm/release && $(CMAKE) -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX:PATH=$(PWD) $(PWD)

build/llvm/debug:
	@mkdir -p build/llvm/debug

build/llvm/release:
	@mkdir -p build/llvm/release

clean:
	@rm -rf build/llvm

.PHONY: all run dbg test tidy format install debug release clean
