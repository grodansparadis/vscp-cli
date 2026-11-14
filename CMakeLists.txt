# CMakeLists.txt
#
# Build instructions for the VSCP command line tools
#
# Copyright (c) 2000-2026 Åke Hedman, Grodans Paradis AB
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# The VSCP library and the header only libraries are fetched at the commits
# pinned below. Build against a local checkout instead with for example
#   -DFETCHCONTENT_SOURCE_DIR_VSCP=../vscp
#
# Windows (dependencies from vcpkg, see .github/workflows/build-windows.yml)
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
#     -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
#     -DVCPKG_TARGET_TRIPLET=x64-windows
#

cmake_minimum_required(VERSION 3.16)

# Version year.month.patch from src/vscp-cli-version.h (updated on every push
# to main by the "Update version" workflow)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/vscp-cli-version.h" _vscpcli_version_lines
     REGEX "^#define[ \t]+VSCPCLI_VERSION_(YEAR|MONTH|PATCH)[ \t]+[0-9]+")
foreach(_part YEAR MONTH PATCH)
  string(REGEX MATCH "VSCPCLI_VERSION_${_part}[ \t]+([0-9]+)" _match "${_vscpcli_version_lines}")
  set(VSCPCLI_VERSION_${_part} "${CMAKE_MATCH_1}")
endforeach()
project(vscp-cli
  VERSION ${VSCPCLI_VERSION_YEAR}.${VSCPCLI_VERSION_MONTH}.${VSCPCLI_VERSION_PATCH}
  DESCRIPTION "VSCP command line tools"
  LANGUAGES CXX C)
message(STATUS "VSCP command line tools version ${PROJECT_VERSION}")

include(FetchContent)

cmake_policy(PUSH)
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(
  vscp
  GIT_REPOSITORY https://github.com/grodansparadis/vscp.git
  GIT_TAG 6763a5b97ecbbfb59f8d67fd3ff78dd8ba9e76d8
)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG 904592d2a86bae5d9c191cdcb8f8fd9b136f71fa
)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG fc7e9c8721b95661cc40e2d1d2329677d2a21387
)
FetchContent_Declare(
  mustache
  GIT_REPOSITORY https://github.com/kainjow/Mustache.git
  GIT_TAG 3f654942a70c46a775070d7a09ca7acfa3e205b7
)
FetchContent_Declare(
  maddy
  GIT_REPOSITORY https://github.com/progsource/maddy.git
  GIT_TAG 4dcdbb50be10a6262072340abcfa97ee3b4edcfa
)

# Only the sources are needed (no add_subdirectory)
foreach(dependency IN ITEMS vscp json spdlog mustache maddy)
  FetchContent_GetProperties(${dependency})
  if(NOT ${dependency}_POPULATED)
    FetchContent_Populate(${dependency})
  endif()
endforeach()
cmake_policy(POP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

if(MSVC)
  add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEANS
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
  )
  add_compile_options(/W3 /Zc:__cplusplus /utf-8)
endif()

# --- System libraries ---

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

if(MSVC)
  find_package(PThreads4W REQUIRED)
  find_package(expat CONFIG REQUIRED)
  find_package(dlfcn-win32 CONFIG REQUIRED)
else()
  find_package(EXPAT REQUIRED)
endif()

include(FindMosquitto)
if(NOT MOSQUITTO_FOUND)
  message(FATAL_ERROR "libmosquitto was not found")
endif()

# Newer libmosquitto headers include cjson/cJSON.h. Homebrew keeps it in
# its own include folder, which is not on the compiler's search path.
find_path(CJSON_INCLUDE_DIR cjson/cJSON.h HINTS /opt/homebrew/include /usr/local/include)
if(CJSON_INCLUDE_DIR)
  list(APPEND MOSQUITTO_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
endif()

# The VSCP library selects OpenSSL API variants from these
foreach(_ssl_ver 1.0 1.1 3.0 3.1 3.2)
  string(FIND "${OPENSSL_VERSION}" "${_ssl_ver}" _ssl_pos)
  if(NOT _ssl_pos EQUAL -1)
    string(REPLACE "." "_" _ssl_def "${_ssl_ver}")
    add_compile_definitions(OPENSSL_API_${_ssl_def})
  endif()
endforeach()

# --- VSCP library and shared command line code ---

set(VSCP_COMMON ${vscp_SOURCE_DIR}/src/vscp/common)
set(VSCP_COMMON2 ${vscp_SOURCE_DIR}/src/common)

add_library(vscpcli-core STATIC
  src/cli/cli-args.cpp
  src/cli/cli-connection.cpp
  src/cli/cli-event.cpp
  src/cli/cli-node.cpp
  src/cli/cli-util.cpp
  ${VSCP_COMMON}/vscpremotetcpif.cpp
  ${VSCP_COMMON}/vscpdatetime.cpp
  ${VSCP_COMMON}/guid.cpp
  ${VSCP_COMMON}/register.cpp
  ${VSCP_COMMON}/mdf.cpp
  ${VSCP_COMMON}/vscphelper.cpp
  ${VSCP_COMMON}/vscpunit.cpp
  ${VSCP_COMMON}/vscpcanaldeviceif.cpp
  ${VSCP_COMMON}/vscphelperlogging.cpp
  ${VSCP_COMMON}/vscphelperlogging-forward.cpp
  ${VSCP_COMMON}/vscp-client-base.cpp
  ${VSCP_COMMON}/vscp-client-canal.cpp
  ${VSCP_COMMON}/vscp-client-level2.cpp
  ${VSCP_COMMON}/vscp-client-mqtt.cpp
  ${VSCP_COMMON}/vscp-client-tcp.cpp
  ${VSCP_COMMON}/vscp-client-ws1.cpp
  ${VSCP_COMMON}/vscp-client-ws2.cpp
  ${VSCP_COMMON}/vscp-websocket.cpp
  ${VSCP_COMMON}/vscp-client-udp.cpp
  ${VSCP_COMMON}/vscp-client-multicast.cpp
  ${VSCP_COMMON2}/vscphelper-common.cpp
  ${VSCP_COMMON2}/vscpbase64.c
  ${VSCP_COMMON2}/vscp-aes.c
  ${VSCP_COMMON2}/crc.c
  ${VSCP_COMMON2}/crc8.c
  ${VSCP_COMMON2}/vscpmd5.c
)

# SocketCAN only exists on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(vscpcli-core PRIVATE ${VSCP_COMMON}/vscp-client-socketcan.cpp)
  target_compile_definitions(vscpcli-core PUBLIC VSCPCLI_HAVE_SOCKETCAN)
endif()

target_compile_definitions(vscpcli-core PUBLIC
  VSCP_ENABLE_HELPER_LOGGING
  VSCP_HELPER_LOGGING_EXPORTS
  VSCP_HELPER_LOGGING_BACKEND_EXPORTS
  VSCPCLI_VERSION_FROM_CMAKE="${PROJECT_VERSION}"
)

target_include_directories(vscpcli-core PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/src/cli
  ${PROJECT_SOURCE_DIR}/src/common
  ${VSCP_COMMON}
  ${VSCP_COMMON2}
  $<$<BOOL:${WIN32}>:${VSCP_COMMON}/win32helpers>
  $<$<BOOL:${WIN32}>:${VSCP_COMMON2}/windows>
  ${json_SOURCE_DIR}/include
  ${spdlog_SOURCE_DIR}/include
  ${mustache_SOURCE_DIR}
  ${maddy_SOURCE_DIR}/include
  ${OPENSSL_INCLUDE_DIR}
  ${MOSQUITTO_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIRS}
)

if(MSVC)
  target_link_libraries(vscpcli-core PUBLIC
    ws2_32
    iphlpapi
    PThreads4W::PThreads4W
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MOSQUITTO_LIBRARIES}
    expat::expat
    CURL::libcurl
    dlfcn-win32::dl
  )
  # Load PThreads4W before VSCP's Windows semaphore header defines fallback macros
  target_compile_options(vscpcli-core PUBLIC
    "$<$<COMPILE_LANGUAGE:CXX>:/FI${PThreads4W_INCLUDE_DIR}/pthread.h>"
  )
else()
  target_include_directories(vscpcli-core PUBLIC ${EXPAT_INCLUDE_DIRS})
  target_link_libraries(vscpcli-core PUBLIC
    ${CMAKE_DL_LIBS}
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${MOSQUITTO_LIBRARIES}
    ${EXPAT_LIBRARIES}
    CURL::libcurl
  )
endif()

# --- The tools ---

set(VSCPCLI_TOOLS vscp-send vscp-dump vscp-gen vscp-reg vscp-info vscp-scan)

foreach(_tool IN LISTS VSCPCLI_TOOLS)
  add_executable(${_tool} src/${_tool}.cpp)
  target_link_libraries(${_tool} PRIVATE vscpcli-core)
endforeach()

install(TARGETS ${VSCPCLI_TOOLS} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/examples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/examples)
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# --- Tests (ctest) ---

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME update_version
             COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/tests/test_update_version.py)
    add_test(NAME tools
             COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/tests/test_tools.py
                     --bindir $<TARGET_FILE_DIR:vscp-send>)
  endif()
endif()
