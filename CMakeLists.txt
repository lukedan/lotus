cmake_minimum_required(VERSION 3.15)
project(pbd)

cmake_policy(SET CMP0092 NEW) # don't add /W3 for MSVC

add_library(pbd STATIC)

target_compile_features(pbd PUBLIC cxx_std_20)

target_sources(pbd
	PUBLIC
		"include/pbd/collision/gjk.h"

		"include/pbd/constraints/face.h"
		"include/pbd/constraints/spring.h"

		"include/pbd/math/constants.h"
		"include/pbd/math/matrix.h"
		"include/pbd/math/quaternion.h"
		"include/pbd/math/vector.h"

		"include/pbd/shapes/shape.h"
		"include/pbd/shapes/sphere.h"

		"include/pbd/body.h"
		"include/pbd/camera.h"
		"include/pbd/common.h"
		"include/pbd/engine.h"
	PRIVATE
		"src/collision/gjk.cpp"
		
		"src/body.cpp")

target_include_directories(pbd
	PUBLIC
		"include/")

# set warning level
if(MSVC)
	target_compile_options(pbd
		PRIVATE /W4 /permissive-)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(pbd
		PRIVATE -Wall -Wextra -Wconversion)
endif()

add_subdirectory("test/")
