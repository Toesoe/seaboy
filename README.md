# seaboy
a gb emulator in plain old c (c-boy)

this emulator is a pet project. I enjoy old tech, and writing emulators is a good way to get familiar with CPU architectures. as of this commit it runs basic 32K roms like Tetris.

I never owned a DMG, but I had a GBC growing up, so color features are coming soom(tm).

if you have SDL2 installed, you can build the project using CMake without any issues, there's no additional dependencies.

feature list:

- ❕ CPU instructions
  - behaviour is inconsistent: still some instructions to go. doesn't yet pass all instruction test ROMs
- ❕ interrupt behaviour
    - timing of EI is off
- ❕ APU
    - as of now, both pulse channels and wave are implemented
    - noise channel TODO
    - output is rough, needs filtering
    - playback is too slow. might be related to global system clocking
    - wavechannel keeps playing after it should be silent
- ✅ PPU
- ✅ full bus
- ❌ mapper support
- ❌ joypad support
    - SDL input handling is not my friend. how can something so easy be so hard? :)
- ❌ GBC support