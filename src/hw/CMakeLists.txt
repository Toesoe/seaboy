target_sources(${SEABOY_EXE} PRIVATE
    cart.c
    cpu.c
    instr.c
    joypad.c
    mem.c
    ppu.c
    #snd.c
)

target_include_directories(${SEABOY_EXE} PUBLIC
    .
)