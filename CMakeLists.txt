cmake_minimum_required(VERSION 2.8.9)

project(prognot_framework)


add_subdirectory(src)

option(${PROJECT_NAME}_BUILD_EXAMPLES "Do you want sample projects to be generated." OFF)

if (${PROJECT_NAME}_BUILD_EXAMPLES)
	add_subdirectory(examples/QtSimpleProgressBar)
	add_subdirectory(examples/QtStackedProgressBar)

	add_dependencies(QtSimpleProgressBar prognot)
	add_dependencies(QtStackedProgressBar prognot)
endif (${PROJECT_NAME}_BUILD_EXAMPLES)

