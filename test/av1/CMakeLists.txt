add_executable(av1_test)
target_sources(av1_test
	PRIVATE
		"main.cpp")
target_link_libraries(av1_test PRIVATE lotus_av1 lotus_helpers)
