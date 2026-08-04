cmake_minimum_required(VERSION 3.15)
project(AmpForge)

add_subdirectory(DPF)
add_subdirectory(plugins/01-gain)
add_subdirectory(plugins/02-amp)
add_subdirectory(plugins/03-screamer)
add_subdirectory(plugins/04-chain)
