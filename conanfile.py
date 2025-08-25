# conanfile.py — consumer recipe (no packaging), Conan 2.x
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class EmulateConan(ConanFile):
    name = "emulate"              # optional for consumers
    version = "0.0.0"             # optional for consumers
    settings = "os", "arch", "compiler", "build_type"

    # Same as your conanfile.txt:
    requires = "catch2/3.5.4"
    generators = "CMakeDeps", "CMakeToolchain"

    # (Optional but handy) Pull CMake/Ninja from Conan instead of system packages
    # tool_requires = ("cmake/3.28.3", "ninja/1.11.1")

    def layout(self):
        # Creates build/<config> dirs and sets CMake source/build folders
        cmake_layout(self)

    def build(self):
        # Standard CMake configure + build using the generated toolchain/deps
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
