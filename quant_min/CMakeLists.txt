cmake_minimum_required(VERSION 3.20)
project(quant_min VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_WARNINGS "Enable extra warnings" ON)
option(ENABLE_SANITIZERS "Enable sanitizers (ASan/UBSan)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

add_executable(quant_min
	src/main.cpp
	src/market/replay.cpp
)

target_include_directories(quant_min PRIVATE include)

if (ENABLE_WARNINGS)
	set_project_warnings(quant_min)
endif()

if (ENABLE_SANITIZERS)
	target_compile_options(quant_min PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(quant_min PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

# Good default optimization for release
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

target_compile_options(quant_min PRIVATE
	$<$<CONFIG:Release>:-O3 -march=native -mtune=native>
)

# for backtest_min
add_executable(backtest_min
  src/backtest_main.cpp
  src/market/replay.cpp
)

target_include_directories(backtest_min PRIVATE include)

if (ENABLE_WARNINGS)
  set_project_warnings(backtest_min)
endif()

if (ENABLE_SANITIZERS)
  target_compile_options(backtest_min PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(backtest_min PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

target_compile_options(backtest_min PRIVATE
  $<$<CONFIG:Release>:-O3 -march=native -mtune=native>
)

add_executable(multi_backtest
  src/multi_backtest_main.cpp
)

target_include_directories(multi_backtest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(multi_backtest PRIVATE Threads::Threads)

add_executable(live_binance_m1
  src/live_binance_m1.cpp
)
target_include_directories(live_binance_m1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(live_binance_m1 PRIVATE Threads::Threads ssl crypto)