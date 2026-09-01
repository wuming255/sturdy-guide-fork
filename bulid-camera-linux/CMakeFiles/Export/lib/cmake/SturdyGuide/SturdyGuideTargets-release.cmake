#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SturdyGuide::sturdy_guide" for configuration "Release"
set_property(TARGET SturdyGuide::sturdy_guide APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(SturdyGuide::sturdy_guide PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsturdy_guide.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS SturdyGuide::sturdy_guide )
list(APPEND _IMPORT_CHECK_FILES_FOR_SturdyGuide::sturdy_guide "${_IMPORT_PREFIX}/lib/libsturdy_guide.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
