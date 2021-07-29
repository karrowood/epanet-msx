#!/bin/bash

printf "\n\n-------- Building MSX Core --------\n\n"

# Build MSX Core
cd MSX\ Core
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . --config Release
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake --build . --config Debug

cd ../..

printf "\n\n-------- Building Legacy DLL --------\n\n"

# Build Legacy DLL
cd "Legacy DLL"
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . --config Release
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake --build . --config Debug

cd ../..

printf "\n\n-------- Building Examples --------\n\n"

# Build CLI
cd example-api
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . --config Release
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake --build . --config Debug

cd ../..

printf "\n\n-------- Building CLI --------\n\n"

# Build CLI
cd CLI
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . --config Release
# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake --build . --config Debug

cd ../..

# Copy legacy.dll and epanet2.dll
cp "Legacy DLL"/build/bin/*/legacy.dll CLI/build/bin/*/
cp ../EPANET/build/bin/*/epanet2.dll CLI/build/bin/*/

# Copy core.dll
cp MSX\ Core/build/bin/*/core.dll CLI/build/bin/*/

# Copy examples.dll
cp example-api/build/bin/*/examples.dll CLI/build/bin/*/

