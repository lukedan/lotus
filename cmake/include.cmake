function(configure_lotus_module TARGET_NAME)
	target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)
	set_target_properties(${TARGET_NAME} PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION ON)
	# set warning level
	if((CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") OR (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
		target_compile_options(${TARGET_NAME}
			PRIVATE /W4 /permissive-)
		target_compile_definitions(${TARGET_NAME}
			PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN
			PRIVATE _CRT_SECURE_NO_WARNINGS)
	elseif((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
		target_compile_options(${TARGET_NAME}
			PUBLIC -Wall -Wextra -Wconversion)
	else()
		message(
			"Unknown compiler ID \"${CMAKE_CXX_COMPILER_ID}\" "
			"with frontend variant \"${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}\".")
	endif()
endfunction()
