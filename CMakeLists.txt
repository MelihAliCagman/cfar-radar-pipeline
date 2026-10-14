cmake_minimum_required(VERSION 3.16)
project(cfar_radar_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Varsayılan olarak Release: sinyal işleme döngüleri optimizasyonsuz çok yavaş.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# Tüm sinyal işleme kodu tek bir statik kütüphanede; demo ve testler buna bağlanır.
add_library(radar STATIC
  src/signal_gen.cpp
)
target_include_directories(radar PUBLIC src)

add_executable(radar_demo src/main.cpp)
target_link_libraries(radar_demo PRIVATE radar)

enable_testing()
add_subdirectory(tests)
