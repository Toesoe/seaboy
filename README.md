# seaboy
a gb emulator in plain old c (c-boy)

this emulator is a pet project. I enjoy old tech, and writing emulators is a good way to get familiar with CPU architectures. as of this commit it runs basic 32K roms like Tetris.

I never owned a DMG, but I had a GBC growing up, so color features are coming soom(tm).

if you have SDL2 installed, you can build the project using CMake without any issues, there's no additional dependencies.

feature list:

- ✅ CPU instructions
  - all pass Blargg testing
- ❕ interrupt behaviour
    - timing of EI is off
- ✅ input handling
- ❕ APU
    - as of now, both pulse channels and wave are implemented
    - noise channel TODO; not sure if it actually produces output :D
    - output is rough, needs decoupling from CPU timebase + filtering
    - wavechannel keeps playing after it should be silent
- ✅ PPU
- ✅ full bus

TODO:
- ❌ mapper support
- ❌ GBC support
- ❓ serial support
    - might not implement this