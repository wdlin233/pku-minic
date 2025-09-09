#!/bin/bash
set -e

case "$1" in
  docker)
    echo "==> Entering Docker development environment..."
    sudo docker run -it -v "$(pwd)":/root/compiler -w /root/compiler maxxing/compiler-dev bash
    ;;
  build)
    echo "==> Building project with CMake..."
    rm -rf build
    cmake -S . -B build
    cmake --build build
    ;;
  debug)
    echo "==> Building project with CMake..."
    rm -rf build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build
    ;;
  # 也可以在 CMakeLists 里构建测试
  test)
    echo "==> Running test: hello.c"
    # 先确保项目已经构建
    if [ ! -f "build/compiler" ]; then
        echo "--> Compiler not found. Building project first..."
        bash "$0" build
    fi
    build/compiler -koopa test/hello.c -o hello.koopa
    echo "==> Test finished. Output is hello.koopa"
    ;;

  autotest)
    echo "==> Running autotest with specific test level $2 with $3 args"
    autotest $3 -s $2 /root/compiler
    ;;
  autotestwd)
    echo "==> Running autotest with specific test level $2 with $3 and -w args"
    autotest -w wd $3 -s $2 /root/compiler
    ;;

  *)
    echo "Usage: $0 {docker}"
    echo "For build and test, please use the standard CMake workflow inside the Docker environment."
    exit 1
    ;;
esac