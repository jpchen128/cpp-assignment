## How to Use
### Dependencies
- Tools: [vcpkg](https://github.com/Microsoft/vcpkg.git), cmake, ninja
- Libraries to install via vcpkg: flatbuffers, boost-asio
```
./bootstrap-vcpkg.sh
./vcpkg integrate install
./vcpkg install flatbuffers boost-asio
```

### Build
```
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake .. -G Ninja
ninja
```

### Run
Start receiver at a port first:
```
./receiver 12345
```
Use sender to send payload to the port:
```
./sender 127.0.0.1 12345
```