cmake_minimum_required(VERSION 3.20)
project(CTR-Native C)
include(CheckCCompilerFlag)

# Single source of truth for the version. The build, the packaged archive name
# and the binary's own banner all read this file, so they cannot disagree --
# package.sh used to carry its own copy of the default.
file(READ "${CMAKE_SOURCE_DIR}/VERSION" CTR_NATIVE_VERSION)
string(STRIP "${CTR_NATIVE_VERSION}" CTR_NATIVE_VERSION)
if(NOT CTR_NATIVE_VERSION)
    message(FATAL_ERROR "VERSION is missing or empty; it is the only place the version is set.")
endif()

# Display name, deliberately separate from the version. CTR_NATIVE_VERSION is
# sent to the leaderboard as the client version and written into replay files as
# ctr_native_version, so changing its value is a compatibility decision rather
# than a cosmetic one. Renaming the product is not.
set(CTR_PRODUCT_NAME "Crash Team Racing: Turbocharged")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "CTR Native currently requires a 32-bit target. "
        "Select Win32 for MSVC, an i686 MinGW compiler on Windows, or pass -m32 "
        "in the initial compiler and linker flags on Linux."
    )
endif()

set(CTR_NATIVE_MSVC_FRONTEND OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    set(CTR_NATIVE_MSVC_FRONTEND ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT EMSCRIPTEN)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
endif()

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE CTR_NATIVE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT CTR_NATIVE_GIT_HASH)
    set(CTR_NATIVE_GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git diff --quiet --exit-code
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CTR_NATIVE_GIT_DIRTY
    ERROR_QUIET
)
if(CTR_NATIVE_GIT_DIRTY EQUAL 0)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}")
else()
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}-dirty")
endif()

# Adds one test executable and registers it with ctest. Extra link sources go
# as plain arguments; arguments to pass to the test binary itself go after
# TEST_ARGS.
function(ctr_native_add_test name source)
    cmake_parse_arguments(ARG "" "" "TEST_ARGS" ${ARGN})
    add_executable(ctr_native_${name}_test ${source} ${ARG_UNPARSED_ARGUMENTS})
    set_target_properties(ctr_native_${name}_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_include_directories(ctr_native_${name}_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_${name}_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_${name}_test PRIVATE m)
    endif()
    add_test(NAME ctr_native_${name} COMMAND ctr_native_${name}_test ${ARG_TEST_ARGS})
endfunction()

function(ctr_native_add_label_warning_if_supported target warning flag_check)
    set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=unknown-warning-option")
    endif()

    check_c_compiler_flag("${warning}" "${flag_check}")
    set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")

    if(${flag_check})
        target_compile_options(${target} PRIVATE "${warning}")
    endif()
endfunction()

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(SDL_OPENGLES ON CACHE BOOL "" FORCE)
else()
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
endif()
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(externals/SDL)

# CTR Native
add_executable(ctr_native main.c)
if(WIN32)
    target_sources(ctr_native PRIVATE platform/windows/ctr_native.rc)
    set_target_properties(ctr_native PROPERTIES OUTPUT_NAME "Crash Team Racing - Turbocharged")
endif()
set_target_properties(ctr_native PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_include_directories(ctr_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(ctr_native PRIVATE
    CTR_NATIVE
    BUILD=926
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_BUILD_ID}\"
    CTR_PRODUCT_NAME=\"${CTR_PRODUCT_NAME}\"
)
if(NOT EMSCRIPTEN)
    target_compile_definitions(ctr_native PRIVATE CTR_INTERNAL)
endif()
if(CTR_NATIVE_MSVC_FRONTEND)
    target_compile_options(ctr_native PRIVATE
        /W4 #TODO(aalhendi): fix warnings
        /bigobj
        /arch:SSE2
        /wd4152
        /wd4244
        /wd4245
        /wd4310
        /wd5287
    )
else()
    target_compile_options(ctr_native PRIVATE
        -Wenum-conversion
        -Wenum-compare
        -Wswitch-enum
        -Wformat
        -Wparentheses
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Release>:-g>
    )

    if(NOT EMSCRIPTEN)
        target_compile_options(ctr_native PRIVATE -msse)
    endif()

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ctr_native PRIVATE -Wall -Wextra -Wstrict-aliasing=2)
        ctr_native_add_label_warning_if_supported(ctr_native -Wfree-labels CTR_HAS_WFREE_LABELS)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        ctr_native_add_label_warning_if_supported(ctr_native -Wc23-extensions CTR_HAS_WC23_EXTENSIONS)
    endif()
endif()
target_link_libraries(ctr_native SDL3::SDL3)
if(EMSCRIPTEN)
    set_target_properties(ctr_native PROPERTIES
        OUTPUT_NAME "ctr-native"
        SUFFIX ".html"
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html"
    )
    target_link_options(ctr_native PRIVATE
        "--shell-file"
        "${CMAKE_SOURCE_DIR}/web/shell.html"
        "--preload-file"
        "${CMAKE_SOURCE_DIR}/assets/dualshock.png@/assets/dualshock.png"
        "-sUSE_WEBGL2=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sFULL_ES3=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=134217728"
        "-sSTACK_SIZE=4194304"
        "-sASYNCIFY=1"
        "-sEMULATE_FUNCTION_POINTER_CASTS=1"
        "-sFORCE_FILESYSTEM=1"
        "-sINVOKE_RUN=0"
        "-sEXIT_RUNTIME=0"
        "-sEXPORTED_RUNTIME_METHODS=['FS','callMain']"
        "-lidbfs.js"
    )
else()
    add_custom_command(TARGET ctr_native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ctr_native>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/dualshock.png"
            "$<TARGET_FILE_DIR:ctr_native>/assets/dualshock.png"
        VERBATIM
    )
    # Optional TrueType fonts for Enhancements > Font; a missing font keeps
    # the retail glyphs.
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/fonts")
        add_custom_command(TARGET ctr_native POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets/fonts"
                "$<TARGET_FILE_DIR:ctr_native>/assets/fonts"
            VERBATIM
        )
    endif()
endif()
if(WIN32)
    target_sources(ctr_native PRIVATE platform/native_http_win32.c)
    target_link_libraries(ctr_native winhttp bcrypt)
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(ctr_native PRIVATE -static-libgcc)
endif()
if(MINGW)
    target_link_options(ctr_native PRIVATE -static)
endif()

include(CTest)
option(CTR_NATIVE_RENDERER_TESTS "Build the renderer integration tests (the gpu-labelled subset needs a GL context; the rest run headless)" OFF)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(ctr_native_unlock_test tests/native_unlock_test.c)
    target_include_directories(ctr_native_unlock_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_unlock_test PRIVATE CTR_NATIVE BUILD=926)
    add_test(NAME ctr_native_unlock COMMAND ctr_native_unlock_test)
    add_executable(ctr_native_engine_test tests/native_engine_test.c)
    target_include_directories(ctr_native_engine_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_engine_test PRIVATE CTR_NATIVE BUILD=926)
    add_test(NAME ctr_native_engine COMMAND ctr_native_engine_test)
    add_executable(ctr_native_engine_metadata_test tests/native_engine_metadata_test.c)
    target_include_directories(ctr_native_engine_metadata_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    add_test(NAME ctr_native_engine_metadata COMMAND ctr_native_engine_metadata_test)
    add_executable(ctr_native_framerate_test tests/native_framerate_test.c)
    target_include_directories(ctr_native_framerate_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_framerate_test PRIVATE CTR_NATIVE BUILD=926)
    add_test(NAME ctr_native_framerate COMMAND ctr_native_framerate_test)
    ctr_native_add_test(options tests/native_options_test.c)
    # Self-contained: it includes platform/native_font.c directly and stubs
    # the asset and renderer layer itself.
    # Font atlas cache round-trip. Needs the fork's font under an assets dir,
    # which .gitignore excludes, so it skips where that file is absent. Point
    # CTR_NATIVE_FONT_ASSET_DIR at a directory containing fonts/crash-a-like.ttf
    # to run it.
    set(CTR_NATIVE_FONT_ASSET_DIR "${CMAKE_SOURCE_DIR}/assets" CACHE PATH
        "Directory containing fonts/crash-a-like.ttf for the font cache test")
    ctr_native_add_test(font_cache tests/native_font_cache_test.c
        TEST_ARGS ${CTR_NATIVE_FONT_ASSET_DIR}
    )
    set_tests_properties(ctr_native_font_cache PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(ctr_native_precision_test
        tests/native_precision_test.c
        platform/native_pgxp.c
        platform/native_draw3d.c
        platform/native_aspect.c
        platform/native_projection.c
        platform/native_visibility_stats.c
        platform/native_gte_core.c
        platform/native_inline_c.c
    )
    target_include_directories(ctr_native_precision_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_precision_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_precision_test PRIVATE m)
    endif()
    add_executable(ctr_native_physics_test tests/native_physics_test.c)
    set_target_properties(ctr_native_physics_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_include_directories(ctr_native_physics_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_physics_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_physics_test PRIVATE m)
    endif()
    add_test(NAME ctr_native_physics COMMAND ctr_native_physics_test)
    add_executable(ctr_native_draw3d_test tests/native_draw3d_test.c platform/native_draw3d.c)
    target_sources(ctr_native_draw3d_test PRIVATE platform/native_aspect.c platform/native_projection.c platform/native_visibility_stats.c)
    target_include_directories(ctr_native_draw3d_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_draw3d_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_draw3d_test PRIVATE m)
    endif()
    add_test(NAME ctr_native_draw3d COMMAND ctr_native_draw3d_test)
    add_executable(ctr_native_aspect_test tests/native_aspect_test.c)
    target_sources(ctr_native_aspect_test PRIVATE platform/native_projection.c)
    set_target_properties(ctr_native_aspect_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_include_directories(ctr_native_aspect_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_aspect_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_aspect_test PRIVATE m)
    endif()
    add_test(NAME ctr_native_aspect COMMAND ctr_native_aspect_test)
    add_executable(ctr_native_projection_test tests/native_projection_test.c)
    set_target_properties(ctr_native_projection_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_include_directories(ctr_native_projection_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(ctr_native_projection_test PRIVATE CTR_NATIVE BUILD=926)
    if(NOT CTR_NATIVE_MSVC_FRONTEND)
        target_link_libraries(ctr_native_projection_test PRIVATE m)
    endif()
    add_test(NAME ctr_native_projection COMMAND ctr_native_projection_test)
    add_test(NAME ctr_native_precision COMMAND ctr_native_precision_test)
    add_test(NAME ctr_native_version COMMAND $<TARGET_FILE:ctr_native> --version)
    set_tests_properties(ctr_native_version PROPERTIES PASS_REGULAR_EXPRESSION "^Crash Team Racing: Turbocharged ")
    if(CTR_NATIVE_RENDERER_TESTS)
        add_executable(ctr_native_depth_renderer_test tests/native_depth_renderer_test.c)
        set_target_properties(ctr_native_depth_renderer_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        target_include_directories(ctr_native_depth_renderer_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
        get_target_property(ctr_game_definitions ctr_native COMPILE_DEFINITIONS)
        get_target_property(ctr_game_options ctr_native COMPILE_OPTIONS)
        get_target_property(ctr_game_libraries ctr_native LINK_LIBRARIES)
        target_compile_definitions(ctr_native_depth_renderer_test PRIVATE ${ctr_game_definitions})
        target_compile_options(ctr_native_depth_renderer_test PRIVATE ${ctr_game_options})
        target_link_libraries(ctr_native_depth_renderer_test PRIVATE ${ctr_game_libraries})
        if(WIN32)
            target_sources(ctr_native_depth_renderer_test PRIVATE platform/native_http_win32.c)
        endif()

        # One executable, one mode flag per check. The modes split into two
        # groups, labelled so CI can select between them:
        #
        #   unlabelled  - never create a GL context. They run anywhere, with no
        #                 display and no GPU, and must not skip: a skip here is
        #                 a real loss of coverage.
        #   LABELS gpu  - call NativeRenderer_InitialiseRender and need a GL
        #                 context. They return 77 where none can be created,
        #                 which ctest treats as a skip rather than a failure.
        #
        # Everything runs on a runner that can provide a context; `ctest -LE gpu`
        # runs the display-independent subset anywhere. Keeping the split in
        # labels rather than in a second build option means the headless checks
        # do not depend on the GPU ones sharing a target.
        set(ctr_gpu_tests
            ctr_native_depth_renderer
            ctr_native_projection_renderer
            ctr_native_aspect_renderer
            ctr_native_hud_icons
            ctr_native_engine_save
        )

        add_test(NAME ctr_native_depth_renderer COMMAND ctr_native_depth_renderer_test)
        # Headless: renders every options-menu row's text for all six languages
        # and every accepted value, before any GL context is created.
        add_test(NAME ctr_native_option_strings COMMAND ctr_native_depth_renderer_test --option-strings-only)
        add_test(NAME ctr_native_aspect_game COMMAND ctr_native_depth_renderer_test --aspect-only)
        add_test(NAME ctr_native_projection_game COMMAND ctr_native_depth_renderer_test --projection-only)
        add_test(NAME ctr_native_projection_renderer COMMAND ctr_native_depth_renderer_test --projection-renderer-only)
        add_test(NAME ctr_native_aspect_renderer COMMAND ctr_native_depth_renderer_test --aspect-renderer-only)
        add_test(NAME ctr_native_oxide_unlock COMMAND ctr_native_depth_renderer_test --oxide-unlock-only)
        add_test(NAME ctr_native_engine_physics COMMAND ctr_native_depth_renderer_test --engine-physics-only)
        add_test(NAME ctr_native_unlock_menu COMMAND ctr_native_depth_renderer_test --unlock-menu-only)
        add_test(NAME ctr_native_preset_input COMMAND ctr_native_depth_renderer_test --preset-input-only)
        add_test(NAME ctr_native_engine_lifecycle COMMAND ctr_native_depth_renderer_test --engine-lifecycle-only)
        add_test(NAME ctr_native_engine_save COMMAND ctr_native_depth_renderer_test --engine-save-only "${CMAKE_CURRENT_BINARY_DIR}/engine-save-test-memcards")
        add_test(NAME ctr_native_hud_icons COMMAND ctr_native_depth_renderer_test --hud-icons-only)
        add_test(NAME ctr_native_minimap COMMAND ctr_native_depth_renderer_test --minimap-only)
        add_test(NAME ctr_native_level_sky_order COMMAND ctr_native_depth_renderer_test --sky-order-only)
        add_test(NAME ctr_native_model_submission COMMAND ctr_native_depth_renderer_test --model-submission-only)
        set_tests_properties(${ctr_gpu_tests} PROPERTIES LABELS gpu SKIP_RETURN_CODE 77)
    endif()
endif()
