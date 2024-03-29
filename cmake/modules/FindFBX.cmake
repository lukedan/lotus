if(NOT DEFINED FBX_SDK_VERSION)
	set(FBX_SDK_VERSION "2020.0.1")
endif()
if(NOT DEFINED FBX_SDK_VS_VERSION)
	set(FBX_SDK_VS_VERSION "vs2017")
endif()
if(NOT DEFINED FBX_SDK_DIR)
	set(FBX_SDK_DIR "C:/Program Files/Autodesk/FBX/FBX SDK/${FBX_SDK_VERSION}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(FBX_SDK_PLATFORM "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(FBX_SDK_PLATFORM "x86")
else()
	message(STATUS "Cannot determine platform for FBX SDK")
endif()

if(DEFINED FBX_SDK_PLATFORM AND IS_DIRECTORY "${FBX_SDK_DIR}")
	set(FBX_SDK_LIB_PATH "${FBX_SDK_DIR}/lib/${FBX_SDK_VS_VERSION}/${FBX_SDK_PLATFORM}")

	set(FBX_INCLUDE_DIR "${FBX_SDK_DIR}/include")
	set(FBX_LIBRARY_DEBUG "${FBX_SDK_LIB_PATH}/debug/libfbxsdk.dll")
	set(FBX_LIBRARY_RELEASE "${FBX_SDK_LIB_PATH}/release/libfbxsdk.dll")
	set(FBX_IMPLIB_DEBUG "${FBX_SDK_LIB_PATH}/debug/libfbxsdk.lib")
	set(FBX_IMPLIB_RELEASE "${FBX_SDK_LIB_PATH}/release/libfbxsdk.lib")

	include(SelectLibraryConfigurations)
	select_library_configurations(FBX)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FBX
	FOUND_VAR FBX_FOUND
	REQUIRED_VARS
		FBX_LIBRARY
		FBX_INCLUDE_DIR)

if(FBX_FOUND AND NOT TARGET FBX::FBX)
	add_library(FBX::FBX SHARED IMPORTED)
	set_target_properties(FBX::FBX PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FBX_INCLUDE_DIR}")
	set_property(TARGET FBX::FBX APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
	set_target_properties(FBX::FBX PROPERTIES IMPORTED_LOCATION_DEBUG "${FBX_LIBRARY_DEBUG}")
	set_target_properties(FBX::FBX PROPERTIES IMPORTED_IMPLIB_DEBUG "${FBX_IMPLIB_DEBUG}")
	set_property(TARGET FBX::FBX APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
	set_target_properties(FBX::FBX PROPERTIES IMPORTED_LOCATION_RELEASE "${FBX_LIBRARY_RELEASE}")
	set_target_properties(FBX::FBX PROPERTIES IMPORTED_IMPLIB_RELEASE "${FBX_IMPLIB_RELEASE}")
endif()
