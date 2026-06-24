project(api)

add_executable(beout_os_api
    main.cpp
    api_server.cpp
)

target_include_directories(beout_os_api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(beout_os_api PRIVATE
    database
    activation
    crypto
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
# Ensure std::filesystem is available (C++17)
set_target_properties(beout_os_api PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS beout_os_api DESTINATION bin)
