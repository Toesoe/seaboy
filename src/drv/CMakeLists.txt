target_sources(${SEABOY_EXE} PRIVATE
    audio.c
    #input.c
    render.c
)

target_include_directories(${SEABOY_EXE} PUBLIC
    .
)