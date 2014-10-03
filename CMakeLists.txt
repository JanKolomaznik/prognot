cmake_minimum_required(VERSION 2.8.9)

project(prognot_framework)


add_subdirectory(src)
add_subdirectory(examples/QtSimpleProgressBar)
add_subdirectory(examples/QtStackedProgressBar)

add_dependencies(QtSimpleProgressBar prognot)
add_dependencies(QtStackedProgressBar prognot)

