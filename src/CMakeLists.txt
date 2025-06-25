target_sources(${SEABOY_EXE} PRIVATE
    cputest.c
    main.c
)

target_include_directories(${SEABOY_EXE} PUBLIC
    .
)

add_subdirectory(drv)
add_subdirectory(hw)
add_subdirectory(lib)