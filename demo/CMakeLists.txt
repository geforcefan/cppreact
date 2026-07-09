cmake_minimum_required(VERSION 3.24)
project(cppreact_demo CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4)

FetchContent_Declare(rmlui
  GIT_REPOSITORY https://github.com/geforcefan/RmlUi.git
  GIT_TAG master)

FetchContent_MakeAvailable(glfw rmlui)

find_package(OpenGL REQUIRED)

add_subdirectory(.. cppreact)

add_executable(todo
  main.cpp
  "${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_GLFW.cpp"
  "${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp"
  "${rmlui_SOURCE_DIR}/Backends/RmlUi_Backend_GLFW_GL3.cpp")

target_include_directories(todo PRIVATE "${rmlui_SOURCE_DIR}/Backends")

target_link_libraries(todo PRIVATE cppreact::cppreact RmlUi::RmlUi glfw OpenGL::GL)

target_compile_definitions(todo PRIVATE
  APP_DOCUMENT="${CMAKE_CURRENT_SOURCE_DIR}/app.rml"
  APP_FONT="${rmlui_SOURCE_DIR}/Samples/assets/LatoLatin-Regular.ttf"
  APP_FONT_BOLD="${rmlui_SOURCE_DIR}/Samples/assets/LatoLatin-Bold.ttf")
