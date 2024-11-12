# Find the native Rocksdb includes and library
# This module defines
#  ROCKSDB_INCLUDE_DIR, where to find rocksdb/db.h, Set when
#                       ROCKSDB_INCLUDE_DIR is found.
#  ROCKSDB_LIBRARIES, libraries to link against to use Rocksdb.
#  ROCKSDB_FOUND, If false, do not try to use Rocksdb.
#  ROCKSDB_VERSION_STRING
#  ROCKSDB_VERSION_MAJOR
#  ROCKSDB_VERSION_MINOR
#  ROCKSDB_VERSION_PATCH
# Get all propreties that cmake supports
if(NOT CMAKE_PROPERTY_LIST)
    execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)
    
    # Convert command output into a CMake list
    string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    string(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
    list(REMOVE_DUPLICATES CMAKE_PROPERTY_LIST)
endif()
    
function(print_properties)
    message("CMAKE_PROPERTY_LIST = ${CMAKE_PROPERTY_LIST}")
endfunction()

function(print_target_properties target)
    if(NOT TARGET ${target})
      message(STATUS "There is no target named '${target}'")
      return()
    endif()

    foreach(property ${CMAKE_PROPERTY_LIST})
        string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" property ${property})

        # Fix https://stackoverflow.com/questions/32197663/how-can-i-remove-the-the-location-property-may-not-be-read-from-target-error-i
        if(property STREQUAL "LOCATION" OR property MATCHES "^LOCATION_" OR property MATCHES "_LOCATION$")
            continue()
        endif()

        get_property(was_set TARGET ${target} PROPERTY ${property} SET)
        if(was_set)
            get_target_property(value ${target} ${property})
            message("${target} ${property} = ${value}")
        endif()
    endforeach()
endfunction()

find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h HINTS ${ROCKSDB_ROOT}/include)
find_library(ROCKSDB_LIBRARIES NAMES librocksdb.a rocksdb HINTS ${ROCKSDB_ROOT}/lib)
find_path(PMEM_INCLUDE_DIR libpmem.h HINTS ${PMEM_ROOT}/include)
find_library(PMEM_LIBRARY NAMES pmem HINTS ${PMEM_ROOT}/lib)
find_library(PMEMOBJ_LIBRARY NAMES pmemobj HINTS ${PMEM_ROOT}/lib)
set(ROCKSDB_THIRDPARTY_LIBS "")
list(APPEND ROCKSDB_THIRDPARTY_LIBS ${PMEM_LIBRARY} ${PMEMOBJ_LIBRARY})
list(APPEND ROCKSDB_THIRDPARTY_LIBS "-lzstd" "-lpthread" "-lsnappy" "-lz" "-lbz2" "-llz4" "-ltbb")

if(ROCKSDB_INCLUDE_DIR AND EXISTS "${ROCKSDB_INCLUDE_DIR}/rocksdb/version.h")
  foreach(ver "MAJOR" "MINOR" "PATCH")
    file(STRINGS "${ROCKSDB_INCLUDE_DIR}/rocksdb/version.h" ROCKSDB_VER_${ver}_LINE
      REGEX "^#define[ \t]+ROCKSDB_${ver}[ \t]+[0-9]+$")
    string(REGEX REPLACE "^#define[ \t]+ROCKSDB_${ver}[ \t]+([0-9]+)$"
      "\\1" ROCKSDB_VERSION_${ver} "${ROCKSDB_VER_${ver}_LINE}")
    unset(${ROCKSDB_VER_${ver}_LINE})
  endforeach()
  set(ROCKSDB_VERSION_STRING
    "${ROCKSDB_VERSION_MAJOR}.${ROCKSDB_VERSION_MINOR}.${ROCKSDB_VERSION_PATCH}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RocksDB
  REQUIRED_VARS ROCKSDB_LIBRARIES ROCKSDB_INCLUDE_DIR
  VERSION_VAR ROCKSDB_VERSION_STRING)

mark_as_advanced(
  ROCKSDB_INCLUDE_DIR
  ROCKSDB_LIBRARIES)

if(RocksDB_FOUND)
  if(NOT TARGET RocksDB::RocksDB)
    add_library(RocksDB::RocksDB UNKNOWN IMPORTED)
    set_target_properties(RocksDB::RocksDB PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS "ON_DCPMM"
      INTERFACE_INCLUDE_DIRECTORIES "${ROCKSDB_INCLUDE_DIR};${PMEM_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${ROCKSDB_THIRDPARTY_LIBS}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      IMPORTED_LOCATION "${ROCKSDB_LIBRARIES}"
      VERSION "${ROCKSDB_VERSION_STRING}")
  endif()
  print_target_properties(RocksDB::RocksDB)
endif()

