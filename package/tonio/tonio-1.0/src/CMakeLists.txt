add_executable(tonio
    http.c
    mfrc522.c
    mfrc522_hal_linux.c
    tonio.c
    volume_mapping.c
    input.c
    ${MEDIA_SRC}
)

target_include_directories(tonio PRIVATE
    ${CMAKE_BINARY_DIR}    # for config.h
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(tonio PRIVATE
    -Wall -Wextra -Wno-unused -pedantic
)

target_link_libraries(tonio PRIVATE
    m
    ${IW_LIBRARY}
    ${MHD_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${CONFUSE_LIBRARIES}
    ${GPIOD_LIBRARIES}
)

# Media-backend-specific libraries
if(MEDIA_BACKEND STREQUAL "vlc")
    target_link_libraries(tonio PRIVATE ${VLC_LIBRARIES})
    target_include_directories(tonio PRIVATE ${VLC_INCLUDE_DIRS})
else()
    target_link_libraries(tonio PRIVATE ${OPUSFILE_LIBRARIES})
    target_include_directories(tonio PRIVATE ${OPUSFILE_INCLUDE_DIRS})
endif()

# pkg-config include dirs (common to both backends)
target_include_directories(tonio PRIVATE
    ${MHD_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${CONFUSE_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
)

# Install tonio binary
install(TARGETS tonio RUNTIME DESTINATION bin)
