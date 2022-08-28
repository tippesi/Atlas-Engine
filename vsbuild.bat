mkdir build
cd build
cmake ../ -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake" -DATLAS_DEMO=ON
start AtlasEngine.sln