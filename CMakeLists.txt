# Dogehiro - a Ghost Squad card editor.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# The window is the system's own web engine, so there is nothing to download:
# WebView2 on Windows (already installed with Edge), WKWebView on macOS,
# WebKitGTK on Linux (package libwebkit2gtk-4.1-dev).

cmake_minimum_required(VERSION 3.16)
project(dogehiro VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_library(gscore STATIC src/gs_card.cpp src/gs_xts.cpp src/gs_png.cpp)
target_include_directories(gscore PUBLIC src third_party)

add_executable(dogehiro WIN32 MACOSX_BUNDLE
  src/main_web.cpp src/gs_dialog.cpp)
target_include_directories(dogehiro PRIVATE third_party/webview)
target_link_libraries(dogehiro PRIVATE gscore)

if(WIN32)
  target_sources(dogehiro PRIVATE res/dogehiro.rc)
  target_include_directories(dogehiro PRIVATE third_party/webview2/include)
  target_link_libraries(dogehiro PRIVATE ole32 oleaut32 shlwapi version comdlg32 advapi32)
elseif(APPLE)
  target_link_libraries(dogehiro PRIVATE "-framework WebKit" "-framework Cocoa")
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-3.0)
  pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.1)
  target_link_libraries(dogehiro PRIVATE PkgConfig::GTK PkgConfig::WEBKIT)
endif()

# The core must keep producing exactly what the reference tools produce.
add_executable(dogehiro-selftest tests/selftest.cpp)
target_link_libraries(dogehiro-selftest PRIVATE gscore)
add_executable(dogehiro-jsontest tests/json_test.cpp)
target_include_directories(dogehiro-jsontest PRIVATE src)
add_executable(dogehiro-export tests/export_tool.cpp)
target_link_libraries(dogehiro-export PRIVATE gscore)

enable_testing()
add_test(NAME json COMMAND dogehiro-jsontest)

install(TARGETS dogehiro RUNTIME DESTINATION . BUNDLE DESTINATION .)
