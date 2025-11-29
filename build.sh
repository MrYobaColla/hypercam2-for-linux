#!/bin/bash
mkdir ~/.hypercam2/
cp icon.png ~/.hypercam2/
cp ur.png ~/.hypercam2/yayko.png
mkdir -p build
cd build
cmake ..
make
cp hypercam2 ..
echo "Build complete! Run ./hypercam2 to start HyperCam 2"
