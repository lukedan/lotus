add_executable(custom_float_test)
configure_lotus_module(custom_float_test)

target_sources(custom_float_test PRIVATE "main.cpp")
target_link_libraries(custom_float_test PRIVATE lotus_core)
