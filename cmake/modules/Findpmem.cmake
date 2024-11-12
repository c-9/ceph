# - Find pmem
#
# pmem_INCLUDE_DIRS - Where to find libpmem headers
# pmem_LIBRARIES - List of libraries when using libpmem.
# pmem_FOUND - True if pmem found.
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

find_package(PkgConfig QUIET REQUIRED)

# all pmem libraries depend on pmem, so always find it
set(pmem_FIND_COMPONENTS ${pmem_FIND_COMPONENTS} pmem)
list(REMOVE_DUPLICATES pmem_FIND_COMPONENTS)

foreach(component ${pmem_FIND_COMPONENTS})
  set(pmem_COMPONENTS pmem pmemobj)
  list(FIND pmem_COMPONENTS "${component}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "unknown libpmem component: ${component}")
  endif()
  pkg_check_modules(PKG_${component} QUIET "lib${component}")
  if(NOT pmem_VERSION_STRING OR PKG_${component}_VERSION VERSION_LESS pmem_VERSION_STRING)
    set(pmem_VERSION_STRING ${PKG_${component}_VERSION})
  endif()
  find_path(pmem_${component}_INCLUDE_DIR
    NAMES lib${component}.h
    HINTS ${PMEM_ROOT}/include ${PKG_${component}_INCLUDE_DIRS})
  find_library(pmem_${component}_LIBRARY
    NAMES ${component}
    HINTS ${PMEM_ROOT}/lib ${PKG_${component}_LIBRARY_DIRS})
  mark_as_advanced(
    pmem_${component}_INCLUDE_DIR
    pmem_${component}_LIBRARY)
  list(APPEND pmem_INCLUDE_DIRS "pmem_${component}_INCLUDE_DIR")
  list(APPEND pmem_LIBRARIES "pmem_${component}_LIBRARY")
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pmem
  REQUIRED_VARS pmem_INCLUDE_DIRS pmem_LIBRARIES
  VERSION_VAR pmem_VERSION_STRING)

mark_as_advanced(
  pmem_INCLUDE_DIRS
  pmem_LIBRARIES)

if(pmem_FOUND)
  foreach(component pmem ${pmem_FIND_COMPONENTS})
    if(NOT TARGET pmem::${component})
      add_library(pmem::${component} UNKNOWN IMPORTED)
      set_target_properties(pmem::${component} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${pmem_${component}_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "C"
        IMPORTED_LOCATION "${pmem_${component}_LIBRARY}")
      # all pmem libraries calls into pmem::pmem
      if(NOT component STREQUAL pmem)
        set_target_properties(pmem::${component} PROPERTIES
          INTERFACE_LINK_LIBRARIES pmem::pmem)
      endif()
    endif()
    print_target_properties(pmem::${component})
  endforeach()
endif()
