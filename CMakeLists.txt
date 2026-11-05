cmake_minimum_required(VERSION 3.15)
project(ACG_Lab4 LANGUAGES CXX)

# Set C++ standard and compiler options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set paths
set(SRC_DIR ${CMAKE_SOURCE_DIR}/Source)
set(SHADER_DIR ${SRC_DIR}/Shaders)
set(DEPS_INCLUDE ${CMAKE_SOURCE_DIR}/Dependencies/Include)  # .h   files
set(DEPS_LIB ${CMAKE_SOURCE_DIR}/Dependencies/Libraries)    # .lib files
set(DEPS_BIN ${CMAKE_SOURCE_DIR}/Dependencies/Binaries)     # .dll files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)

# Find source files (they are all in this folder)
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Set compiler warnings / colors
add_compile_options(-Wall -Wno-deprecated -g)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fansi-escape-codes -fcolor-diagnostics)
endif()

# Add includes ---
target_include_directories(${PROJECT_NAME} PRIVATE ${DEPS_INCLUDE})

# Link dependencies based on OS
if(WIN32)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME}
        ${DEPS_LIB}/glfw3.lib
        ${DEPS_LIB}/glew32.lib
        OpenGL::GL
    )

    file(GLOB DLL_FILES "${DEPS_BIN}/*.dll")
    if(DLL_FILES)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${DLL_FILES} $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying runtime DLLs..."
        )
    endif()
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 QUIET)
    find_package(GLEW QUIET)

    if(NOT glfw3_FOUND)
        message(FATAL_ERROR
            "\nGLFW not found.\n"
            "Install it with: brew install glfw\n"
            "Or ensure it is available to CMake.\n"
        )
    endif()

    if (NOT GLEW_FOUND)
        message(FATAL_ERROR
            "\nGLEW not found.\n"
            "Install it with: brew install glew\n"
            "Or ensure it’s available to CMake.\n"
        )
    endif()

    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    find_library(COREVID_LIB CoreVideo REQUIRED)
    find_library(COREFOUND_LIB CoreFoundation REQUIRED)

    target_link_libraries(${PROJECT_NAME}
        glfw
        GLEW::GLEW
        OpenGL::GL
        ${COCOA_LIB}
        ${IOKIT_LIB}
        ${COREVID_LIB}
        ${COREFOUND_LIB}
    )
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 3.3 QUIET)
    find_package(GLEW QUIET)

    if(NOT glfw3_FOUND)
        message(FATAL_ERROR
            "\nGLFW not found.\n"
            "Install it with: sudo apt install libglfw3-dev\n"
        )
    endif()

    if (NOT GLEW_FOUND)
        message(FATAL_ERROR
            "\nGLEW not found.\n"
            "Install it with: sudo apt install libglew-dev\n"
        )
    endif()

    target_link_libraries(${PROJECT_NAME}
        OpenGL::GL
        glfw
        GLEW::GLEW
    )
endif()

# Copy shaders after build
file(GLOB SHADER_FILES
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_FILES} $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying shader files to runtime directory..."
)

# Copy resources folder after build
set(RESOURCES_DIR ${CMAKE_SOURCE_DIR}/Resources)

if(IS_DIRECTORY "${RESOURCES_DIR}")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RESOURCES_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources"
        COMMENT "Copying Resources folder to output directory..."
        VERBATIM
    )
endif()

# run target (build + copy + execute)
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running ${PROJECT_NAME}..."
)

set(CMAKE_VERBOSE_MAKEFILE ON)
