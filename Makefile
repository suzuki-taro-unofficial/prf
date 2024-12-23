build: prepare_cmake
	cd build && make -j

clean:
	rm -rf build

test: build
	cd build && CTEST_OUTPUT_ON_FAILURE=1 make test


prepare_cmake:
	cmake -S . -B build
