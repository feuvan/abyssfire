# AbyssCore tests (ue58-platform.md 4.6, ARCHITECTURE 3.3).
#
# Builds the portable core (Source/AbyssCore Public + Private minus Private/UE) as a static library with the same
# language rules UE uses (C++20, no exceptions, no RTTI, no FP contraction) plus -Werror warnings, and one doctest
# runner. Each area is registered with ctest separately (doctest test suites).
cmake_minimum_required(VERSION 3.20)
project(AbyssCoreTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(ABYSS_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
# Builds AbyssCore as a shared library with hidden visibility (-fvisibility=hidden, ABYSS_CORE_DLL=1) and links the tests
# against it: a symbol used by the tests but missing ABYSS_API fails to link, the way UE modular (editor / Windows DLL)
# builds would (ue58-platform.md 4.4; Platform.h ABYSS_API).
option(ABYSS_SHARED "Build AbyssCore as a hidden-visibility shared library (export check)" OFF)

get_filename_component(ABYSS_UNREAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(ABYSS_CORE_DIR "${ABYSS_UNREAL_DIR}/Source/AbyssCore")

# -Wundef / /we4668: UE treats an undefined identifier in #if as an error on several platforms (ue58-platform.md 3.3).
if(MSVC)
  set(ABYSS_CORE_FLAGS /W4 /WX /we4668 /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /fp:precise /utf-8)
else()
  set(ABYSS_CORE_FLAGS -Wall -Wextra -Wshadow -Wundef -Werror -Wno-unused-parameter -fno-exceptions -fno-rtti
      -ffp-contract=off)
endif()

if(ABYSS_SANITIZE AND NOT MSVC)
  # float-cast-overflow is not part of GCC's -fsanitize=undefined (Math.h SaturatingInt32 keeps the core clean of it).
  add_compile_options(-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer
                      -fno-sanitize-recover=undefined,float-cast-overflow)
  add_link_options(-fsanitize=address,undefined,float-cast-overflow)
endif()

# ---- the core library -----------------------------------------------------------------------------------------------
file(GLOB_RECURSE ABYSS_CORE_SOURCES CONFIGURE_DEPENDS "${ABYSS_CORE_DIR}/Private/*.cpp")
list(FILTER ABYSS_CORE_SOURCES EXCLUDE REGEX ".*/Private/UE/.*")
file(GLOB_RECURSE ABYSS_CORE_HEADERS CONFIGURE_DEPENDS "${ABYSS_CORE_DIR}/Public/*.h" "${ABYSS_CORE_DIR}/Private/*.h")

if(ABYSS_SHARED)
  add_library(AbyssCore SHARED ${ABYSS_CORE_SOURCES} ${ABYSS_CORE_HEADERS})
  set_target_properties(AbyssCore PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
  target_compile_definitions(AbyssCore PUBLIC ABYSS_CORE_DLL=1)
  if(NOT MSVC)
    target_link_options(AbyssCore PRIVATE -Wl,--no-undefined)
  endif()
else()
  add_library(AbyssCore STATIC ${ABYSS_CORE_SOURCES} ${ABYSS_CORE_HEADERS})
endif()
target_include_directories(AbyssCore PUBLIC "${ABYSS_CORE_DIR}/Public")
# Mirrors AbyssCore.Build.cs: ABYSS_CORE_BUILDING marks the core's own translation units (export side, FP tripwire).
target_compile_definitions(AbyssCore PRIVATE ABYSS_CORE_BUILDING=1)
target_compile_options(AbyssCore PRIVATE ${ABYSS_CORE_FLAGS})

# ---- tests ----------------------------------------------------------------------------------------------------------
file(GLOB ABYSS_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(abyss_tests ${ABYSS_TEST_SOURCES})
target_link_libraries(abyss_tests PRIVATE AbyssCore)
target_include_directories(abyss_tests SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_include_directories(abyss_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_definitions(abyss_tests PRIVATE
  DOCTEST_CONFIG_NO_EXCEPTIONS
  DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS  # a failed REQUIRE aborts the run (no exceptions)
  DOCTEST_CONFIG_SUPER_FAST_ASSERTS
  ABYSS_UNREAL_DIR="${ABYSS_UNREAL_DIR}")
target_compile_options(abyss_tests PRIVATE ${ABYSS_CORE_FLAGS})

# ---- macro shim (compile-only) ---------------------------------------------------------------------------------------
# shim/MacroShim.cpp compiles every public header after the Windows / UE / Apple macros a game-module TU sees
# (ue58-platform.md 3.3): a collision fails the build. The two generated headers list the public headers' <...>
# includes (pulled in before the macros, as CoreMinimal.h would) and the public headers themselves.
file(GLOB_RECURSE ABYSS_PUBLIC_HEADERS CONFIGURE_DEPENDS RELATIVE "${ABYSS_CORE_DIR}/Public" "${ABYSS_CORE_DIR}/Public/*.h")
list(SORT ABYSS_PUBLIC_HEADERS)
set(ABYSS_SHIM_STD "")
set(ABYSS_SHIM_PUBLIC "")
foreach(h IN LISTS ABYSS_PUBLIC_HEADERS)
  file(STRINGS "${ABYSS_CORE_DIR}/Public/${h}" std_lines REGEX "^#include <[^>]+>")
  foreach(line IN LISTS std_lines)
    string(REGEX MATCH "<[^>]+>" inc "${line}")
    list(APPEND ABYSS_SHIM_STD "${inc}")
  endforeach()
  string(APPEND ABYSS_SHIM_PUBLIC "#include \"${h}\"\n")
endforeach()
list(REMOVE_DUPLICATES ABYSS_SHIM_STD)
list(SORT ABYSS_SHIM_STD)
set(ABYSS_SHIM_STD_TEXT "")
foreach(inc IN LISTS ABYSS_SHIM_STD)
  string(APPEND ABYSS_SHIM_STD_TEXT "#include ${inc}\n")
endforeach()
set(ABYSS_SHIM_GEN "${CMAKE_CURRENT_BINARY_DIR}/shim")
file(CONFIGURE OUTPUT "${ABYSS_SHIM_GEN}/abyss_shim_std.h" CONTENT "// generated by CMakeLists.txt\n${ABYSS_SHIM_STD_TEXT}")
file(CONFIGURE OUTPUT "${ABYSS_SHIM_GEN}/abyss_shim_public.h" CONTENT "// generated by CMakeLists.txt\n${ABYSS_SHIM_PUBLIC}")
add_library(abyss_macro_shim OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/shim/MacroShim.cpp")
target_include_directories(abyss_macro_shim PRIVATE "${ABYSS_CORE_DIR}/Public" "${ABYSS_SHIM_GEN}")
target_compile_definitions(abyss_macro_shim PRIVATE ABYSS_CORE_DLL=1)  # consumer side of a modular build
target_compile_options(abyss_macro_shim PRIVATE ${ABYSS_CORE_FLAGS})
# Build it whenever the tests build (run.sh builds the default target).
add_dependencies(abyss_tests abyss_macro_shim)

enable_testing()
# One ctest entry per area (doctest TEST_SUITE names). Keep in sync with the suites used in tests/*.cpp.
set(ABYSS_TEST_AREAS base data hero combat monsters items quests story world pets audio save sim)
foreach(area IN LISTS ABYSS_TEST_AREAS)
  add_test(NAME core.${area} COMMAND abyss_tests --test-suite=${area} --no-intro --no-version)
  set_tests_properties(core.${area} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()
