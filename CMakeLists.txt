cmake_minimum_required(VERSION 3.21.0)

project(seaboy
    DESCRIPTION "seaboy emulator"
    LANGUAGES C
    VERSION "0.1"
)

set(TARGET_FILE "seaboy")
set(SEABOY_EXE ${TARGET_FILE}.elf)

add_executable(${TARGET_FILE}.elf)

target_compile_features(${SEABOY_EXE} PUBLIC c_std_23)

set(CMAKE_C_FLAGS_DEBUG     "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_CXX_FLAGS_DEBUG   "-Og -ggdb3 -gdwarf-5 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -fstack-usage")
set(CMAKE_C_FLAGS_RELEASE   "-Os -s -flto -fno-fat-lto-objects -fno-ident -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fno-stack-protector")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -s -flto -fno-fat-lto-objects -fno-ident -fmerge-constants -fno-asynchronous-unwind-tables -fmerge-all-constants -fno-stack-protector")

set(CMAKE_GENERATOR "Ninja")

# cascading definitions
add_compile_definitions(
    $<$<CONFIG:Debug>:DEBUG>
    CMAKE_EXPORT_COMPILE_COMMANDS=1
)

add_subdirectory(src)

target_link_libraries(${SEABOY_EXE} PUBLIC SDL2)