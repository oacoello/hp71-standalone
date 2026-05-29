cmake_minimum_required(VERSION 3.10)

project(hp71_emulator)

set(CMAKE_CXX_STANDARD 17)

add_executable(hp71_emulator

main.cpp

../saturn_core/saturn_cpu.cpp
../saturn_core/saturn_memory.cpp
../saturn_core/keyboard_matrix.cpp
../saturn_core/saturn_decoder.cpp
../saturn_core/saturn_alu.cpp
../saturn_core/saturn_field.cpp
../saturn_core/saturn_shift.cpp
../saturn_core/saturn_flags.cpp
../saturn_core/saturn_timer.cpp
../saturn_core/hardware_map.cpp
../saturn_core/hp71_lcd.cpp

../saturn_core/basic_engine.cpp
../saturn_core/ballistic_engine.cpp

)

target_include_directories(hp71_emulator PRIVATE
../saturn_core
../external
)
