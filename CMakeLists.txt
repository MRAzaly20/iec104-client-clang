cmake_minimum_required(VERSION 3.16)
project(iec104_client_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# lib60870 — pre-built static library
# ============================================================================
set(LIB60870_ROOT "${CMAKE_SOURCE_DIR}/../lib60870/lib60870-C")
set(LIB60870_SRC "${LIB60870_ROOT}/src")

set(LIB60870_INCLUDE_DIRS
  "${LIB60870_SRC}/inc/api"
  "${LIB60870_SRC}/inc/internal"
  "${LIB60870_SRC}/common/inc"
  "${LIB60870_SRC}/hal/inc"
)
set(LIB60870_LIB "${LIB60870_SRC}/liblib60870.a")

# ============================================================================
# cJSON — single-file library
# ============================================================================
add_library(cjson STATIC
  "${CMAKE_SOURCE_DIR}/lib/cJSON/cJSON.c"
)
target_include_directories(cjson PUBLIC
  "${CMAKE_SOURCE_DIR}/lib/cJSON"
)

# ============================================================================
# iec104_core — shared modules (model, config, log, conn, cmd)
# ============================================================================
add_library(iec104_core STATIC
  src/iec104_model.c
  src/iec104_config.c
  src/iec104_log.c
  src/iec104_conn.c
  src/iec104_cmd.c
)
target_include_directories(iec104_core PUBLIC
  src
  ${LIB60870_INCLUDE_DIRS}
)
target_link_libraries(iec104_core PUBLIC
  cjson
  ${LIB60870_LIB}
)
if(WIN32)
  target_link_libraries(iec104_core PUBLIC ws2_32)
else()
  target_link_libraries(iec104_core PUBLIC pthread m)
endif()

# ============================================================================
# Phase 1 test
# ============================================================================
add_executable(test_phase1 src/test_phase1.c)
target_link_libraries(test_phase1 PRIVATE iec104_core)

# ============================================================================
# Console client (Phase 2)
# ============================================================================
add_executable(iec104_client_console src/main.c)
target_link_libraries(iec104_client_console PRIVATE iec104_core)

# ============================================================================
# GTK4 GUI client (Phase 3+)
# ============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

add_executable(iec104_client_gui
  src/main_gui.c
  src/iec104_gui.c
  src/iec104_sld.c
)
target_include_directories(iec104_client_gui PRIVATE
  ${GTK4_INCLUDE_DIRS}
)
target_link_directories(iec104_client_gui PRIVATE
  ${GTK4_LIBRARY_DIRS}
)
target_link_libraries(iec104_client_gui PRIVATE
  iec104_core
  ${GTK4_LIBRARIES}
)
target_compile_options(iec104_client_gui PRIVATE
  ${GTK4_CFLAGS_OTHER}
)

# Copy CSS to build directory for easy testing
file(COPY "${CMAKE_SOURCE_DIR}/res/style.css"
  DESTINATION "${CMAKE_BINARY_DIR}/res/")
