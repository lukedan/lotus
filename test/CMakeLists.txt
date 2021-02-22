add_executable(testbed)

target_sources(testbed
	PRIVATE "testbed.cpp")
target_link_libraries(testbed PRIVATE pbd)
