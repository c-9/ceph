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

find_path(KVDK_INCLUDE_DIR NAMES "kvdk/engine.hpp" HINTS $(KVDK_ROOT)/include)
find_library(KVDK_LIB NAMES libengine.so HINTS $(KVDK_ROOT)/lib)
find_path(PMEM_INCLUDE_DIR libpmem.h HINTS ${PMEM_ROOT}/include)
find_library(PMEM_LIBRARY NAMES pmem HINTS ${PMEM_ROOT}/lib)
find_library(PMEMOBJ_LIBRARY NAMES pmemobj HINTS ${PMEM_ROOT}/lib)
set(KVDK_THIRDPARTY_LIBS "")
list(APPEND KVDK_THIRDPARTY_LIBS ${PMEM_LIBRARY} ${PMEMOBJ_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KVDK
  REQUIRED_VARS KVDK_LIB KVDK_INCLUDE_DIR)

mark_as_advanced(
  KVDK_LIB
  KVDK_INCLUDE_DIR)

if(KVDK_FOUND)
  if(NOT TARGET KVDK)
    add_library(KVDK UNKNOWN IMPORTED)
    set_target_properties(KVDK PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${KVDK_INCLUDE_DIR};${PMEM_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${ROCKSDB_INCLUDE_DIR};${PMEM_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      IMPORTED_LOCATION "${KVDK_LIB}")
    print_target_properties(KVDK)
  endif()
endif()