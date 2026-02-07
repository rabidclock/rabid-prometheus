cmake_minimum_required(VERSION 3.20)
project(prometheus_head
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Prometheus Head — Bi-Cameral Mind Backplane"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies — all optional so the scaffold builds without full stack
# ---------------------------------------------------------------------------

# llama.cpp — linked directly for the Lizard (Phi-3.5 embedded inference)
find_library(LLAMA_LIB llama HINTS ${LLAMA_CPP_DIR}/lib ENV LLAMA_CPP_DIR)
find_path(LLAMA_INCLUDE llama.h HINTS ${LLAMA_CPP_DIR}/include ENV LLAMA_CPP_DIR)
if(LLAMA_LIB AND LLAMA_INCLUDE)
    message(STATUS "Found llama.cpp: ${LLAMA_LIB}")
    set(HAS_LLAMA ON)
else()
    message(STATUS "llama.cpp not found — Lizard inference stubbed")
    set(HAS_LLAMA OFF)
endif()

# ZeroMQ — IPC with the mineflayer body
# Dev headers may be absent (no sudo), so we fetch zmq.h via FetchContent
# and link against the system libzmq.so.5.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZMQ QUIET libzmq)
endif()
if(NOT ZMQ_FOUND)
    # Try finding the library directly; also check for versioned .so.5
    find_library(ZMQ_LIB NAMES zmq
        PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
    if(NOT ZMQ_LIB)
        # No unversioned symlink — look for libzmq.so.5 directly
        find_file(ZMQ_LIB NAMES libzmq.so.5
            PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib
            NO_DEFAULT_PATH)
    endif()
    if(ZMQ_LIB)
        # Try system header first
        find_path(ZMQ_INC zmq.h)
        if(NOT ZMQ_INC)
            # Fetch zmq.h from the libzmq 4.3.5 release
            include(FetchContent)
            FetchContent_Declare(libzmq_headers
                URL https://github.com/zeromq/libzmq/archive/refs/tags/v4.3.5.tar.gz
            )
            FetchContent_Populate(libzmq_headers)
            set(ZMQ_INC "${libzmq_headers_SOURCE_DIR}/include")
            message(STATUS "Fetched zmq.h from libzmq 4.3.5 source")
        endif()
        set(ZMQ_FOUND ON)
        set(ZMQ_LIBRARIES ${ZMQ_LIB})
        set(ZMQ_INCLUDE_DIRS ${ZMQ_INC})
        message(STATUS "Found ZeroMQ: lib=${ZMQ_LIB}  inc=${ZMQ_INC}")
    else()
        message(STATUS "ZeroMQ not found — BodyLink stubbed")
    endif()
endif()

# libcurl — async HTTP to llama-server (Soul) and Gemini API (Teacher)
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "Found libcurl: ${CURL_LIBRARIES}")
else()
    message(STATUS "libcurl dev not found — HTTP calls stubbed")
endif()

# nlohmann/json — lightweight JSON handling (FetchContent as fallback)
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
    message(STATUS "nlohmann/json fetched via FetchContent")
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

add_executable(prometheus_head
    src/main.cpp
    src/lizard/lizard.cpp
    src/soul/soul.cpp
    src/arbiter/arbiter.cpp
    src/ipc/body_link.cpp
    src/memory/memory.cpp
    src/circadian/circadian.cpp
    src/teacher/teacher.cpp
)

target_include_directories(prometheus_head PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(prometheus_head PRIVATE
    nlohmann_json::nlohmann_json
    pthread
)

# Conditionally link optional deps
if(HAS_LLAMA)
    target_include_directories(prometheus_head PRIVATE ${LLAMA_INCLUDE})
    target_link_libraries(prometheus_head PRIVATE ${LLAMA_LIB})
    target_compile_definitions(prometheus_head PRIVATE HAS_LLAMA=1)
endif()

if(ZMQ_FOUND)
    target_include_directories(prometheus_head PRIVATE ${ZMQ_INCLUDE_DIRS})
    target_link_libraries(prometheus_head PRIVATE ${ZMQ_LIBRARIES})
    target_compile_definitions(prometheus_head PRIVATE HAS_ZMQ=1)
endif()

if(CURL_FOUND)
    target_link_libraries(prometheus_head PRIVATE CURL::libcurl)
    target_compile_definitions(prometheus_head PRIVATE HAS_CURL=1)
endif()

target_compile_options(prometheus_head PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
)
