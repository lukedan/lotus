add_executable(job_system_test)
configure_lotus_module(job_system_test)

target_sources(job_system_test PRIVATE "main.cpp")
target_link_libraries(job_system_test PRIVATE lotus_core lotus_utils)
