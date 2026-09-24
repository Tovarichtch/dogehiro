# Dogehiro, a Ghost Squad card editor.
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

cmake_minimum_required(VERSION 3.16)
project(dogehiro VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # Visual Studio: no runtime DLL to ship
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# The page and the window icon are compiled into the program.
set(GEN ${CMAKE_BINARY_DIR}/gen)
function(embed file header symbol)
  add_custom_command(OUTPUT ${GEN}/${header}
    COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_SOURCE_DIR}/${file} -DOUT=${GEN}/${header}
            -DNAME=${symbol} ${ARGN} -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS ${file} cmake/embed.cmake VERBATIM)
endfunction()
embed(ui/index.html ui_embed.h UI_INDEX_HTML -DTEXT=ON)
embed(res/doge.png gs_icon.h GS_ICON_PNG)

add_library(gscore STATIC src/gs_xts.cpp src/gs_png.cpp)
target_include_directories(gscore PUBLIC src)

add_executable(dogehiro WIN32 MACOSX_BUNDLE
  src/main_web.cpp src/gs_dialog.cpp ${GEN}/ui_embed.h ${GEN}/gs_icon.h)
target_include_directories(dogehiro PRIVATE third_party/webview ${GEN})
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

add_executable(dogehiro-selftest tests/selftest.cpp)    # dogehiro-selftest <game image>
target_link_libraries(dogehiro-selftest PRIVATE gscore)
add_executable(dogehiro-jsontest tests/json_test.cpp)
target_include_directories(dogehiro-jsontest PRIVATE src)
add_executable(dogehiro-export tests/export_tool.cpp)   # dogehiro-export <game image> <assets dir>
target_link_libraries(dogehiro-export PRIVATE gscore)

enable_testing()
add_test(NAME json COMMAND dogehiro-jsontest)

install(TARGETS dogehiro RUNTIME DESTINATION . BUNDLE DESTINATION .)
