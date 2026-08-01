cmake_minimum_required(VERSION 3.16)
project(GalacticConquest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Core simulation library: pure C++, no external dependencies.
# ---------------------------------------------------------------------------
add_library(gc_core STATIC
  src/core/Types.cpp
  src/data/Database.cpp
  src/data/UnitDefs.cpp
  src/data/BuildingDefs.cpp
  src/data/TechDefs.cpp
  src/data/GalaxyDefs.cpp
  src/sim/GameState.cpp
  src/sim/Autoresolve.cpp
  src/sim/AI.cpp
  src/battle/Tactical.cpp
)
target_include_directories(gc_core PUBLIC src)

# ---------------------------------------------------------------------------
# Graphical client (SDL2). Optional: if SDL2 is missing we still build the
# console client and the tests.
# ---------------------------------------------------------------------------
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
  endif()
endif()

if(SDL2_FOUND)
  add_executable(galactic-conquest
    src/ui/Font.cpp
    src/ui/Gfx.cpp
    src/ui/App.cpp
    src/ui/MenuScreen.cpp
    src/ui/GalaxyScreen.cpp
    src/ui/BattleScreen.cpp
    src/app/main.cpp
  )
  target_link_libraries(galactic-conquest PRIVATE gc_core)
  if(TARGET SDL2::SDL2)
    target_link_libraries(galactic-conquest PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
      target_link_libraries(galactic-conquest PRIVATE SDL2::SDL2main)
    endif()
  else()
    target_include_directories(galactic-conquest PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(galactic-conquest PRIVATE ${SDL2_LIBRARIES})
    target_link_directories(galactic-conquest PRIVATE ${SDL2_LIBRARY_DIRS})
  endif()
  message(STATUS "SDL2 found - building graphical client 'galactic-conquest'")
else()
  message(WARNING "SDL2 not found - only the console client and tests will be built")
endif()

# ---------------------------------------------------------------------------
# Console client: plays the whole campaign from a terminal, no dependencies.
# ---------------------------------------------------------------------------
add_executable(galactic-conquest-console src/app/console_main.cpp)
target_link_libraries(galactic-conquest-console PRIVATE gc_core)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(gc_tests tests/test_sim.cpp)
target_link_libraries(gc_tests PRIVATE gc_core)
add_test(NAME sim_tests COMMAND gc_tests)
