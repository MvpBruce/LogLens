cmake_minimum_required(VERSION 3.21)
project(LogLens LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Requires a Qt 6 SDK. Point CMake at it with either:
#   -DCMAKE_PREFIX_PATH=C:/Qt/6.8.1/msvc2022_64
# or set the Qt6_DIR env var. See README for install via aqtinstall.
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(LogLens
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
  src/LogModel.cpp
  src/LogModel.h
  src/LogFilterProxy.cpp
  src/LogFilterProxy.h
  src/LogDelegate.cpp
  src/LogDelegate.h
  src/LogLoader.cpp
  src/LogLoader.h)

target_link_libraries(LogLens PRIVATE Qt6::Widgets)

# Keep the console attached for now so qDebug() is visible during development.
# Flip to WIN32_EXECUTABLE ON before shipping a release build.
set_target_properties(LogLens PROPERTIES WIN32_EXECUTABLE OFF)
