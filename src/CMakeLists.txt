cmake_minimum_required(VERSION 2.8.9)

project(prognot)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11")
find_package(Boost)


# Find the QtWidgets library
find_package(Qt5Widgets)

set(${PROJECT_NAME}_INCLUDE_DIR "../")

set(${PROJECT_NAME}_INCLUDES
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/prognot.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/ProgressNotifier.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/ProgressBar.hpp
	${${PROJECT_NAME}_INCLUDE_DIR}/prognot/Qt/StackedProgressBar.hpp
	)

set(${PROJECT_NAME}_SOURCES
	"StackedProgressBar.cpp"
	#${${PROJECT_NAME}_BINARY_DIR}/prognot_automoc.cpp
	)

message("${${PROJECT_NAME}_BINARY_DIR}/prognot_automoc.cpp")
include_directories(
	${${PROJECT_NAME}_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	"../"
	)

add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_INCLUDES} ${${PROJECT_NAME}_INCLUDES})

# Use the Widgets module from Qt 5.
qt5_use_modules(${PROJECT_NAME} Widgets)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
