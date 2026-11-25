cmake_minimum_required(VERSION 3.28...4.2)
project(prognot VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Boost 1.85 REQUIRED CONFIG)
find_package(Threads REQUIRED)

add_library(prognot INTERFACE)
add_library(prognot::prognot ALIAS prognot)
target_compile_features(prognot INTERFACE cxx_std_20)
target_include_directories(prognot INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(prognot INTERFACE Boost::headers Threads::Threads)

option(BUILD_TESTING "Build prognot tests" ${PROJECT_IS_TOP_LEVEL})
include(CTest)
if(BUILD_TESTING)
    add_executable(prognot_tests tests/core.cpp)
    target_link_libraries(prognot_tests PRIVATE prognot::prognot)
    set_target_properties(prognot_tests PROPERTIES CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(prognot_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    foreach(case IN ITEMS "batch and scope" "hierarchy" "validation" "moves and lifetime"
            "cancellation and failures" "iterators" "concurrent children" "deep arithmetic"
            "callback contract" "early iterator exit")
        string(REPLACE " " "_" test_name "${case}")
        add_test(NAME prognot.${test_name} COMMAND prognot_tests "${case}")
    endforeach()
endif()

include(CMakePackageConfigHelpers)
install(TARGETS prognot EXPORT prognotTargets)
install(FILES prognot/prognot.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/prognot)
install(EXPORT prognotTargets NAMESPACE prognot:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prognot)
configure_package_config_file(cmake/prognotConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/prognotConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prognot)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/prognotConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/prognotConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/prognotConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/prognot)
