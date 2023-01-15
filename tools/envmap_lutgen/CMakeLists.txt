add_executable(envmap_lutgen)
configure_lotus_module(envmap_lutgen)

target_sources(envmap_lutgen PRIVATE "src/main.cpp")
target_link_libraries(envmap_lutgen PRIVATE lotus_core)
