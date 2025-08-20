# Generate GCC Debug profile
set -x CC gcc
set -x CXX g++
conan profile detect --force --name=gcc-debug
sed -i 's/build_type=.*/build_type=Debug/' ~/.conan2/profiles/gcc-debug

# Generate GCC Release profile
set -x CC gcc
set -x CXX g++
conan profile detect --force --name=gcc-release
sed -i 's/build_type=.*/build_type=Release/' ~/.conan2/profiles/gcc-release

# Generate Clang Debug profile
set -x CC clang-20
set -x CXX clang++-20
conan profile detect --force --name=clang-debug
sed -i 's/build_type=.*/build_type=Debug/' ~/.conan2/profiles/clang-debug

# Generate Clang Release profile
set -x CC clang-20
set -x CXX clang++-20
conan profile detect --force --name=clang-release
sed -i 's/build_type=.*/build_type=Release/' ~/.conan2/profiles/clang-release
