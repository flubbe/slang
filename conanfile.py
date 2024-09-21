import os
from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, cmake_layout


class slangRecipe(ConanFile):
    name = "slang"
    version = "0.1"

    # Optional metadata
    license = "MIT"
    author = "Felix Lubbe"
    url = ""
    description = "A simple scripting language."
    topics = "Scripting language"

    # Binary configuration
    settings = "os", "arch", "compiler", "build_type"

    # Configure header-only library.
    exports_sources = "include/*"

    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("fmt/[>=10.0.0 <11.0]")
        self.test_requires("gtest/[>=1.14.0 <2.0]")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.22.6 <4.0]")

    def validate(self):
        check_min_cppstd(self, 17)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
