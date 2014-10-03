cmake_minimum_required(VERSION 2.8.9)

project(prognot)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")
set(Boost_USE_STATIC_LIBS   ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost)


# Find the QtWidgets library
find_package(Qt5Widgets)

set(${PROJECT_NAME}_INCLUDE_DIR "../")

include_directories(
	${${PROJECT_NAME}_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	"../"
	)

set(${PROJECT_NAME}_INCLUDES
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/prognot.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/ProgressNotifier.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/ProgressBar.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/StackedProgressBar.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/ProgressDialog.hpp
	)

set(${PROJECT_NAME}_SOURCES
	"StackedProgressBar.cpp"
	"ProgressDialog.cpp"
	)

qt5_wrap_ui(${PROJECT_NAME}_UIS
	"ProgressDialog.ui"
	)

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_INCLUDES} ${${PROJECT_NAME}_UIS})
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX _d )

# Use the Widgets module from Qt 5.
qt5_use_modules(${PROJECT_NAME} Widgets)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
