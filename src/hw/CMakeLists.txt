target_sources(${SEABOY_EXE} PRIVATE
    cart.c
    cpu.c
    instr.c
    joypad.c
    mem.c
    ppu.c
    apu.c
)

target_include_directories(${SEABOY_EXE} PUBLIC
    .
)