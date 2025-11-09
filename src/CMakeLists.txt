target_sources(${SEABOY_EXE} PRIVATE
    main.c
)

target_include_directories(${SEABOY_EXE} PUBLIC
    .
)

add_subdirectory(drv)
add_subdirectory(hw)