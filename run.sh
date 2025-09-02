#!/bin/bash
set -e

case "$1" in
  docker)
    echo "==> Entering Docker development environment..."
    sudo docker run -it -v "$(pwd)":/root/compiler -w /root/compiler maxxing/compiler-dev bash
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

  *)
    echo "Usage: $0 {docker}"
    echo "For build and test, please use the standard CMake workflow inside the Docker environment."
    exit 1
    ;;
esac