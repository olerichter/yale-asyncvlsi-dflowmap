#!/usr/bin/env bash
./configure
if [ ! -d build ]; then
	mkdir build
fi
cd build
cmake  ..
make -j
