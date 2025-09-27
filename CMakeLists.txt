
cmake_minimum_required(VERSION 3.25)

# Set the path to the toolchain file, which will configure CMake to use the MIPS
# toolchain rather than its default compiler and proceed in turn to execute
# setup.cmake.
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/ps1-bare-metal/cmake/toolchain.cmake")

set(PROJECT_NAME "picostation-menu")

# Tell CMake about the project. The VERSION, DESCRIPTION and HOMEPAGE_URL fields
# are optional, but the project name and LANGUAGES field should be present.
project(
    ${PROJECT_NAME}
    LANGUAGES    C CXX ASM
    VERSION      1.0.0
    DESCRIPTION  "PSX.Dev project template"
    HOMEPAGE_URL "https://github.com/grumpycoders/pcsx-redux"
)

# Set up compiler flags and initialize the Python environment used to run the
# scripts in the tools directory.
include(ps1-bare-metal/cmake/setup.cmake)
include(ps1-bare-metal/cmake/virtualenv.cmake)

# Build a "common" library containing basic support code. We are going to link
# this library into our executable.
add_library(
    common OBJECT
    ps1-bare-metal/libc/clz.s
    ps1-bare-metal/libc/crt0.c
    ps1-bare-metal/libc/cxxsupport.cpp
    ps1-bare-metal/libc/malloc.c
    ps1-bare-metal/libc/misc.c
    ps1-bare-metal/libc/setjmp.s
    ps1-bare-metal/libc/string.c
    ps1-bare-metal/libc/string.s
    ps1-bare-metal/ps1/cache.s
    ps1-bare-metal/vendor/printf.c
    src/psxproject/system.s
    src/psxproject/unhandledexc.c
)
target_include_directories(
    common PUBLIC
    ps1-bare-metal/
    ps1-bare-metal/libc
)

# Compile the main executable. You may add more source files by listing them
# here.
add_executable(
    ${PROJECT_NAME}
    src/gpu.c
    src/main.c
    src/file_manager.c
    src/controller.c
    src/psxproject/cdrom.c
    src/psxproject/delay.c
    src/psxproject/filesystem.c
    src/psxproject/irq.c
    src/psxproject/system.c
    src/psxproject/stream.c
    src/psxproject/spu.c
)
target_link_libraries(${PROJECT_NAME} PRIVATE common)

# Define a CMake macro that invokes convertImage.py in order to generate VRAM
# texture data from an image file.
function(convertImage input bpp)
    add_custom_command(
        OUTPUT  ${ARGN}
        DEPENDS "${PROJECT_SOURCE_DIR}/${input}"
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/ps1-bare-metal/tools/convertImage.py"
            -b ${bpp}
            "${PROJECT_SOURCE_DIR}/${input}"
            ${ARGN}
        VERBATIM
    )
endfunction()

# Convert the font spritesheet to a 4bpp texture and palette, then embed them
# into the executable. The addBinaryFile() macro is defined in setup.cmake; you
# may call it multiple times to embed other data into the binary.
convertImage(assets/images/font.png 4 fontTexture.dat fontPalette.dat)
convertImage(assets/images/picostationlogo.png 4 logoTexture.dat logoPalette.dat)
convertImage(assets/images/background_template.png 4 backgroundTexture.dat backgroundPalette.dat)
convertImage(assets/images/icon_disc.png 4 iconDiscTexture.dat iconDiscPalette.dat)
convertImage(assets/images/icon_settings.png 4 iconSettingsTexture.dat iconSettingsPalette.dat)
convertImage(assets/images/icon_folder.png 4 iconFolderTexture.dat iconFolderPalette.dat)
addBinaryFile(${PROJECT_NAME} fontTexture "${PROJECT_BINARY_DIR}/fontTexture.dat")
addBinaryFile(${PROJECT_NAME} fontPalette "${PROJECT_BINARY_DIR}/fontPalette.dat")
addBinaryFile(${PROJECT_NAME} logoTexture "${PROJECT_BINARY_DIR}/logoTexture.dat")
addBinaryFile(${PROJECT_NAME} logoPalette "${PROJECT_BINARY_DIR}/logoPalette.dat")
addBinaryFile(${PROJECT_NAME} backgroundTexture "${PROJECT_BINARY_DIR}/backgroundTexture.dat")
addBinaryFile(${PROJECT_NAME} backgroundPalette "${PROJECT_BINARY_DIR}/backgroundPalette.dat")
addBinaryFile(${PROJECT_NAME} iconDiscTexture "${PROJECT_BINARY_DIR}/iconDiscTexture.dat")
addBinaryFile(${PROJECT_NAME} iconDiscPalette "${PROJECT_BINARY_DIR}/iconDiscPalette.dat")
addBinaryFile(${PROJECT_NAME} iconSettingsTexture "${PROJECT_BINARY_DIR}/iconSettingsTexture.dat")
addBinaryFile(${PROJECT_NAME} iconSettingsPalette "${PROJECT_BINARY_DIR}/iconSettingsPalette.dat")
addBinaryFile(${PROJECT_NAME} iconFolderTexture "${PROJECT_BINARY_DIR}/iconFolderTexture.dat")
addBinaryFile(${PROJECT_NAME} iconFolderPalette "${PROJECT_BINARY_DIR}/iconFolderPalette.dat")
addBinaryFile(${PROJECT_NAME} click_sfx "${PROJECT_SOURCE_DIR}/assets/click.vag")
addBinaryFile(${PROJECT_NAME} slide_sfx "${PROJECT_SOURCE_DIR}/assets/slide.vag")

# Add a step to run convertExecutable.py after the executable is compiled in
# order to convert it into a PS1 executable. By default all custom commands run
# from the build directory, so paths to files in the source directory must be
# prefixed with ${PROJECT_SOURCE_DIR}.
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    BYPRODUCTS ${PROJECT_NAME}.psexe
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/ps1-bare-metal/tools/convertExecutable.py"
        "$<TARGET_FILE:${PROJECT_NAME}>"
        ${PROJECT_NAME}.psexe
    VERBATIM
)

# ==============================================================================
# Image Generation Post-Build Step
# Generates .psexe image from compiled binary using platform-specific scripts
# ==============================================================================

# Define script based on platform - fail fast with clear error message
if(WIN32)
    set(IMAGE_SCRIPT "${PROJECT_SOURCE_DIR}/make-xiso.bat")
    set(PLATFORM_NAME "Windows")
else()
    set(IMAGE_SCRIPT "${PROJECT_SOURCE_DIR}/makeimage.sh")
    set(PLATFORM_NAME "Unix/Linux")
endif()

# Validate script exists before proceeding
if(NOT EXISTS "${IMAGE_SCRIPT}")
    message(FATAL_ERROR 
        "Image generation script not found for ${PLATFORM_NAME}: ${IMAGE_SCRIPT}\n"
        "Please ensure the appropriate script exists in the project root."
    )
endif()

# Generate image post-build
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND 
		"${IMAGE_SCRIPT}"
        "$<TARGET_FILE:${PROJECT_NAME}>"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

# Optional: Add dependency tracking for the script itself
# This ensures rebuild if the script changes
set_property(
    TARGET ${PROJECT_NAME}
    APPEND PROPERTY ADDITIONAL_CLEAN_FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.psexe"
)
