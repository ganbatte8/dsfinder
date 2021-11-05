#!/bin/bash
COMPILER_FLAGS="-DDEBUG=0 -DCOMPILER_GCC -O2 -Wall -Wno-pedantic -Wextra -Werror -fno-rtti -Wno-switch -Wno-logical-not-parentheses -Wno-unused-parameter -Wno-write-strings -Wno-unused-function -Wno-unused-variable -Wno-maybe-uninitialized -fno-exceptions -std=gnu++11"

STRICT_FLAGS="-g -DDEBUG=1 -DCOMPILER_GCC -O0 -Wall -Wno-pedantic -Wextra -Werror -fno-rtti -fno-exceptions -std=gnu++11 -Wno-write-strings"

mkdir -p ../build
#g++ -o ../build/dsfinder_asset_packer dsfinder_asset_packer.cpp $COMPILER_FLAGS

g++ -shared -o ../build/dsfinder.willbeso -fPIC dsfinder.cpp $COMPILER_FLAGS
mv ../build/dsfinder.willbeso ../build/dsfinder.so
g++ linux_dsfinder.cpp -o ../build/linux_dsfinder -ldl -lpthread -lxcb -lX11-xcb -lGL -lX11 $COMPILER_FLAGS

#gcc -o ../build/walker walk.c -Wall -Wpedantic -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -g -O0