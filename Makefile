all: build

build:
	cmake -DCMAKE_BUILD_TYPE=Debug -B build
	cmake --build build

clean:
	rm -rf build

docker:
	sudo docker run -it -v /home/wdlin/sysy-cmake-template:/root/compiler maxxing/compiler-dev bash

test:
	build/compiler -koopa test/hello.c -o hello.koopa

.PHONY: all build clean docker test
