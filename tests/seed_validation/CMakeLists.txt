cmake_minimum_required(VERSION 3.20)
project(ngb_seed_validation LANGUAGES CXX)
find_package(glslang CONFIG REQUIRED)
set(NGB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(ngb-seed-validate
  "${NGB_ROOT}/tests/d3d_seed_validate.cpp"
  "${NGB_ROOT}/src/d3d/gpu/shaders.cpp"
  "${NGB_ROOT}/src/d3d/vk/spirv.cpp")
target_compile_features(ngb-seed-validate PRIVATE cxx_std_20)
target_compile_definitions(ngb-seed-validate PRIVATE NGB_SEED_COMPILE=1 NGB_SMOOTH_MOTION=1)
target_include_directories(ngb-seed-validate PRIVATE "${NGB_ROOT}/src/d3d")
target_link_libraries(ngb-seed-validate PRIVATE glslang::glslang glslang::glslang-default-resource-limits)
