# Included only for the real iPhoneOS game target.
enable_language(OBJC)
if(NOT TARGET SDL2::SDL2main)
    message(FATAL_ERROR "The iOS game requires SDL2's UIKit main entry point")
endif()
set(NGB_MOLTENVK_ROOT "" CACHE PATH "Extracted official MoltenVK distribution")
set(NGB_MOLTENVK_FRAMEWORK "${NGB_MOLTENVK_ROOT}/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework")
if(NOT EXISTS "${NGB_MOLTENVK_FRAMEWORK}/MoltenVK")
    message(FATAL_ERROR "An iPhoneOS MoltenVK framework is required; set NGB_MOLTENVK_ROOT")
endif()
set_property(SOURCE src/main.c APPEND PROPERTY COMPILE_DEFINITIONS main=SDL_main)
set(NGB_IOS_BUILD_COMMIT "local" CACHE STRING "Source revision embedded in the app")
set(NGB_IOS_BUILD_VERSION "2.0" CACHE STRING "Distinct LiveContainer update version")
target_sources(${PROJECT_NAME} PRIVATE ios/app.m)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    NGB_IOS_BUILD_COMMIT="${NGB_IOS_BUILD_COMMIT}"
    NGB_IOS_BUILD_VERSION="${NGB_IOS_BUILD_VERSION}")
target_compile_definitions(${PROJECT_NAME} PRIVATE NGB_IOS_APP=1)
target_compile_definitions(ngb_gen PRIVATE NGB_IOS_APP=1)
if(NGB_IOS_KOREAN)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen/recomp_korean.c")
        message(FATAL_ERROR "Korean function variants must be prepared privately first")
    endif()
    set(NGB_KOREAN_ASSET_DIR "" CACHE PATH "Private verified Korean patch capsule")
    if(NOT EXISTS "${NGB_KOREAN_ASSET_DIR}/manifest.json")
        message(FATAL_ERROR "The verified private Korean data capsule is required")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE ios/korean.m)
    target_sources(ngb_gen PRIVATE ios/game_hooks.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NGB_IOS_KOREAN=1)
    target_compile_definitions(ngb_gen PRIVATE NGB_IOS_KOREAN=1)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${NGB_KOREAN_ASSET_DIR}"
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/KoreanPatch" VERBATIM)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${XBOXRECOMP_DIR}/third_party/volk ${XBOXRECOMP_DIR}/third_party/Vulkan-Headers/include)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main
    "-framework UIKit" "-framework Foundation" "-framework GameController"
    "-framework AVFoundation" "-framework UniformTypeIdentifiers" "-framework QuartzCore" "-framework CoreText")
target_link_options(${PROJECT_NAME} PRIVATE "-Wl,-rpath,@executable_path/Frameworks")
set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
    XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ios/build_icons.swift"
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/appimage/ngb.png")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND xcrun --sdk macosx swift "${CMAKE_CURRENT_SOURCE_DIR}/ios/build_icons.swift"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/appimage/ngb.png"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/ios-icons"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${NGB_MOLTENVK_FRAMEWORK}"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Frameworks/MoltenVK.framework"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/LICENSES/ngbrecomp.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY.md"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/LICENSES/THIRD_PARTY.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${NGB_MOLTENVK_ROOT}/LICENSE"
        "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/LICENSES/MoltenVK.txt"
    VERBATIM)
