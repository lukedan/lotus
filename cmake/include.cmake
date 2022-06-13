function(configure_lotus_module TARGET_NAME)
	target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)
	set_target_properties(${TARGET_NAME} PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION ON)
	# set warning level
	if(MSVC)
		target_compile_options(${TARGET_NAME}
			PRIVATE /W4 /permissive-)
		target_compile_definitions(${TARGET_NAME}
			PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN
			PRIVATE _CRT_SECURE_NO_WARNINGS)
	elseif(CMAKE_COMPILER_IS_GNUCXX)
		target_compile_options(${TARGET_NAME}
			PUBLIC -Wall -Wextra -Wconversion)
	endif()
endfunction()
