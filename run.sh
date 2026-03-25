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
  test)
    cmake --build build
    build/compiler $2 $3 -o temp.s
    cat $3
    echo "==> Test finished. Output is temp.s"
    ;;

  autotest)
    echo "==> Running autotest with specific test level $3 with $2 args"
    autotest $2 -s $3 /root/compiler
    ;;
  autotestwd)
    echo "==> Running autotest with specific test level $3 with $2 and -w args"
    autotest -w wd $2 -s $3 /root/compiler
    ;;

  help)
    echo "Use run.sh test: bash run.sh test -koopa /opt/bin/testcases/lv8/02_params.c"
    echo "Use run.sh autotest: bash run.sh autotest -koopa lv8"
    ;;

  *)
    echo "Usage: $0 {docker}"
    echo "For build and test, please use the standard CMake workflow inside the Docker environment."
    exit 1
    ;;
esac