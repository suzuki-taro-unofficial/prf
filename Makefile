build: prepare_cmake
	cd build && make -j

build-debug: prepare_debug_cmake
	cd build && make -j

clean:
	rm -rf build

test: build
	cd build && CTEST_OUTPUT_ON_FAILURE=1 make test

test-debug: build-debug
	cd build && CTEST_OUTPUT_ON_FAILURE=1 make test

test-verbose: build
	cd build && ctest --verbose

test-verbose-debug: build-debug
	cd build && ctest --verbose


prepare_cmake:
	cmake -S . -B build -DPRF_SHOW_INFO_LOG=ON -DPRF_SHOW_WARN_LOG=ON -DPRF_DEBUG=OFF

prepare_debug_cmake:
	cmake -S . -B build -DPRF_SHOW_INFO_LOG=ON -DPRF_SHOW_WARN_LOG=ON -DPRF_DEBUG=ON
