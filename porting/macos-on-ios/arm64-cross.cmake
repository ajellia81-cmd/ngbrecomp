# Intel Mac host, unchanged ARM64 macOS engine target. No iPhoneOS engine build.
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
set(CMAKE_OSX_SYSROOT macosx CACHE STRING "" FORCE)
