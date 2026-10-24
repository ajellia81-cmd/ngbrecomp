# Match vcpkg's device triplet; only Release binaries are needed by the IPA.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_OSX_DEPLOYMENT_TARGET 15.0)
# Static libintl uses CF locale functions. Propagate its framework dependency
# to Meson's iOS utility links as well as CMake's target checks.
set(VCPKG_LINKER_FLAGS "-framework CoreFoundation")
