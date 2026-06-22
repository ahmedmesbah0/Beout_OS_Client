project(provisioning)

add_executable(beout_os_provisioning 
    main.cpp
    cli_engine.cpp
)

target_include_directories(beout_os_provisioning PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(beout_os_provisioning PRIVATE database)

install(TARGETS beout_os_provisioning DESTINATION bin)
