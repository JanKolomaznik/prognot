find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_library(prognot_qt STATIC ProgressNotifier.cpp ProgressDialog.cpp StackedProgressBar.cpp PopupProgressBar.cpp
    ../prognot/Qt/ProgressNotifier.hpp ../prognot/Qt/ProgressDialog.hpp
    ../prognot/Qt/ProgressBar.hpp ../prognot/Qt/StackedProgressBar.hpp ../prognot/Qt/PopupProgressBar.hpp)
add_library(prognot::qt ALIAS prognot_qt)
set_target_properties(prognot_qt PROPERTIES AUTOMOC ON EXPORT_NAME qt POSITION_INDEPENDENT_CODE ON)
target_link_libraries(prognot_qt PUBLIC prognot::prognot Qt6::Widgets)
install(TARGETS prognot_qt EXPORT prognotTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ../prognot/Qt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/prognot)
