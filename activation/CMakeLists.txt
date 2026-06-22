project(activation)

add_library(activation
    machine_id.cpp
    activation_manager.cpp
)

target_include_directories(activation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(activation PUBLIC crypto database)
