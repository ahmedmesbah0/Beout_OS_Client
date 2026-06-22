project(database)

add_library(database 
    database_manager.cpp
)

target_include_directories(database PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(database PUBLIC SQLite::SQLite3)
