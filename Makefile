release:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DTESTS=false -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Release --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

debug:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DTESTS=true -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`

test:
	cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Debug -DTESTS=true -DCMAKE_INSTALL_PREFIX:STRING=${PREFIX} -S . -B ./build
	cmake --build ./build --config Debug --target all -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
	ctest --test-dir ./build --output-on-failure

clear:
	rm -rf build

install:
	cmake --build build --target install

all:
	$(MAKE) clear
	$(MAKE) release
