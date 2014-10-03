cmake_minimum_required(VERSION 2.8.9)

project(QtStackedProgressBar)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11")
# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Boost)

set(${PROJECT_NAME}_INCLUDES
	)

set(${PROJECT_NAME}_SOURCES
	"main.cpp"
	)

include_directories(
	${${PROJECT_NAME}_BINARY_DIR}
	${Boost_INCLUDE_DIR}
	"../../"
	)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} ${PROJECT_RESOURCES} ${${PROJECT_NAME}_UIS} ${${PROJECT_NAME}_INCLUDES})

# Use the Widgets module from Qt 5.
qt5_use_modules(${PROJECT_NAME} Widgets)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} prognot)
