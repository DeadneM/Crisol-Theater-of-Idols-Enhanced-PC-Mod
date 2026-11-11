cmake_minimum_required(VERSION 3.24)
project(CRToiEnhanced LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Build requires 64-bit Windows")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(minhook URL https://github.com/TsudaKageyu/minhook/archive/refs/tags/v1.3.4.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(imgui minhook)
add_library(crtoi_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(crtoi_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
add_library(crtoi_dxgi SHARED dxgi_proxy.cpp)
set_target_properties(crtoi_dxgi PROPERTIES OUTPUT_NAME "dxgi" PREFIX "")
target_link_libraries(crtoi_dxgi PRIVATE user32)
add_library(crtoi_mod SHARED crtoi_asi.cpp HubReentry.cpp)
set_target_properties(crtoi_mod PROPERTIES OUTPUT_NAME "CRToiEnhanced" PREFIX "" SUFFIX ".asi")
target_include_directories(crtoi_mod PRIVATE ${CMAKE_SOURCE_DIR}/../src)
target_link_libraries(crtoi_mod PRIVATE crtoi_imgui minhook d3d11 d3d12 d3dcompiler dxgi user32 xinput9_1_0 bcrypt)
enable_testing()
add_executable(hub_policy_tests tests/HubPolicyTests.cpp)
add_test(NAME completed_chapter_access_policy COMMAND hub_policy_tests)
if(MSVC)
  target_compile_options(crtoi_dxgi PRIVATE /W4 /EHsc)
  target_compile_options(crtoi_mod PRIVATE /W4 /EHsc)
endif()
