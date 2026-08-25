cmake_minimum_required(VERSION 3.15)
project(MioProgetto VERSION 1.0 LANGUAGES CXX)

# Forza l'utilizzo dello standard C++ 17 o superiore
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Genera il file compile_commands.json per il supporto dell'IntelliSense nell'editor
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Definisce l'eseguibile e i file sorgenti associati
add_executable(mio_programma main.cpp)
