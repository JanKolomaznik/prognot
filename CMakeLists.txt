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
    foreach(case IN ITEMS batch_and_scope hierarchy validation moves_and_lifetime
            cancellation_and_failures iterators concurrent_children deep_arithmetic
            callback_contract early_iterator_exit constrained_callbacks)
        add_test(NAME prognot.${case} COMMAND prognot_tests --run_test=${case})
    endforeach()
endif()

option(PROGNOT_BUILD_QT "Build Qt 6 widgets and adapter" OFF)
option(PROGNOT_BUILD_EXAMPLES "Build Qt examples (requires PROGNOT_BUILD_QT)" OFF)
if(PROGNOT_BUILD_QT)
    add_subdirectory(src)
    if(BUILD_TESTING)
        add_executable(prognot_qt_tests tests/qt.cpp)
        target_link_libraries(prognot_qt_tests PRIVATE prognot::qt)
        foreach(case IN ITEMS worker_delivery_and_lifecycle exact_weighted_aggregation
                cancellation_and_clear widget_destruction_during_work dialog_outcomes_and_buttons
                nested_core_tasks receiver_deleted_from_signal clear_with_pending_events
                popup_interaction_and_placement)
            add_test(NAME prognot.qt.${case} COMMAND prognot_qt_tests --run_test=${case})
            set_tests_properties(prognot.qt.${case} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
        endforeach()
    endif()
endif()
if(PROGNOT_BUILD_EXAMPLES)
    if(NOT PROGNOT_BUILD_QT)
        message(FATAL_ERROR "PROGNOT_BUILD_EXAMPLES requires PROGNOT_BUILD_QT")
    endif()
    add_subdirectory(examples/QtSimpleProgressBar)
    add_subdirectory(examples/QtStackedProgressBar)
endif()

include(CMakePackageConfigHelpers)
install(TARGETS prognot EXPORT prognotTargets)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/prognot)
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
