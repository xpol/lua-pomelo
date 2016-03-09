cmake_minimum_required(VERSION 3.0)

project (pomelo C)

list(APPEND CMAKE_MODULE_PATH "cmake/Modules")

# compile in release  mode by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(WITH_SHARED_LIBUV "Link to a shared libuv library instead of static linking" OFF)
option(WITH_UV_TCP_TRANS "Link to a shared libuv library instead of static linking" ON)
option(WITH_UV_TLS_TRANS "Link to a shared libuv library instead of static linking" ON)
set(BUILD_TESTS OFF CACHE BOOL "Build tests")

if(UNIX)
  if(APPLE)
    set(PLAT "macosx")
    set(LINK_FLAGS "-bundle -undefined dynamic_lookup -all_load")
  else(APPLE)
    set(PLAT "linux")
    if(BUILD_SHARED_LIBS)
      set(LINK_FLAGS "-shared")
    endif()
  endif(APPLE)
  add_definitions(-D_GNU_SOURCE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -Wall -fPIC -march=native -Wno-unused-parameter")
else(UNIX)
  if(WIN32)
    set(PLAT "win32")
    add_definitions(-DLUA_BUILD_AS_DLL -DLUA_LIB _D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /TP")
  else(WIN32)
    set(PLAT "unsupported")
  endif(WIN32)
endif(UNIX)

if (NOT LUA_INCLUDE_DIR OR NOT (BUILD_SHARED_LIBS OR LUA_LIBRARIES))
    message("Using system lua...")
    set(ENV{LUA_DIR} ~/.luaenv/versions/5.1.5) # hints for find lua in luaenv.
    find_package(Lua REQUIRED)
endif()

if (NOT LIBPOMELO2_DIR)
  set(LIBPOMELO2_DIR deps/libpomelo2)
endif()


find_package(LibUV)
find_package(OpenSSL)

include_directories(${LUA_INCLUDE_DIR} ${LIBPOMELO2_DIR}/include ${LIBPOMELO2_DIR}/src ${LIBUV_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

list(APPEND SOURCES
  src/lua-pomelo.c
  ${LIBPOMELO2_DIR}/src/pc_pomelo.c
  ${LIBPOMELO2_DIR}/src/pc_lib.c
  ${LIBPOMELO2_DIR}/src/pc_trans.c
  ${LIBPOMELO2_DIR}/src/pc_trans_repo.c
  ${LIBPOMELO2_DIR}/src/pc_JSON.c
  ${LIBPOMELO2_DIR}/src/tr/dummy/tr_dummy.c
)

if (WITH_UV_TCP_TRANS)
  list(APPEND SOURCES
    ${LIBPOMELO2_DIR}/src/tr/uv/pr_msg.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pr_msg_json.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pr_pkg.c
    ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tcp.c
    ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tcp_i.c
    ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tcp_aux.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pr_msg_pb.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pb_i.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pb_decode.c
    ${LIBPOMELO2_DIR}/src/tr/uv/pb_encode.c
  )
  if (OPENSSL_FOUND)
    list(APPEND SOURCES
      ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tls.c
      ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tls_i.c
      ${LIBPOMELO2_DIR}/src/tr/uv/tr_uv_tls_aux.c
    )
  else()
    add_definitions(-DPC_NO_UV_TLS_TRANS)
  endif()
else()
  add_definitions(-DPC_NO_UV_TCP_TRANS)
endif()


if(BUILD_SHARED_LIBS)
  add_library(pomelo MODULE ${SOURCES})
  target_link_libraries(pomelo ${LUA_LIBRARIES} ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})
  set_target_properties(pomelo PROPERTIES OUTPUT_NAME "pomelo" PREFIX "")
else()
  add_library(pomelo ${SOURCES})
  set_target_properties(pomelo PROPERTIES OUTPUT_NAME "luapomelo")
endif()

if (LINK_FLAGS)
  set_target_properties(pomelo PROPERTIES LINK_FLAGS ${LINK_FLAGS})
endif()

if (CMAKE_INSTALL_PREFIX)
  install(TARGETS pomelo DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endif()

if (BUILD_TESTS)
  include_directories(src)
  add_definitions(-DPROJECT_ROOT="${CMAKE_SOURCE_DIR}")
  add_executable(runner spec/runner.c)
  message("Lua: ${LUA_LIBRARIES}")
  target_link_libraries(runner pomelo ${LUA_LIBRARIES} ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})
endif()
