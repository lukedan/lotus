add_executable(auto_diff_test)
target_sources(auto_diff_test
    PRIVATE
        "main.cpp")
target_link_libraries(auto_diff_test PRIVATE lotus_core)
