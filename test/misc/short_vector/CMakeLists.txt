add_executable(short_vector_test)
configure_lotus_module(short_vector_test)

target_sources(short_vector_test PRIVATE "main.cpp")
target_link_libraries(short_vector_test PRIVATE lotus_core)
