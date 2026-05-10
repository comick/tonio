add_executable(check_json check_json.c)

target_include_directories(check_json PRIVATE
    ${CMAKE_SOURCE_DIR}/src    # for json.h
    ${CHECK_INCLUDE_DIRS}
)

target_link_libraries(check_json PRIVATE
    ${CHECK_LIBRARIES}
)

# Register test
add_test(NAME check_json COMMAND check_json)
