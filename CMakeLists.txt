cmake_minimum_required(VERSION 3.16)
project(kaleidoscope-llvm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(kaleido src/main.cpp src/parser.cpp src/codegen.cpp src/runtime.cpp)
llvm_map_components_to_libnames(LLVM_LIBS core orcjit native passes support)
target_link_libraries(kaleido ${LLVM_LIBS})
# Export the runtime functions so the JIT can find them by name.
set_target_properties(kaleido PROPERTIES ENABLE_EXPORTS ON)
