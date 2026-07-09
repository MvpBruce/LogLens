cmake_minimum_required(VERSION 3.21)
project(LogLens LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Requires a Qt 6 SDK. Point CMake at it with either:
#   -DCMAKE_PREFIX_PATH=C:/Qt/6.8.1/msvc2022_64
# or set the Qt6_DIR env var. See README for install via aqtinstall.
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()
enable_testing()
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

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
  src/LogLoader.h
  src/LogTailer.cpp
  src/LogTailer.h)

target_link_libraries(LogLens PRIVATE Qt6::Widgets)

# Keep the console attached for now so qDebug() is visible during development.
# Flip to WIN32_EXECUTABLE ON before shipping a release build.
set_target_properties(LogLens PROPERTIES WIN32_EXECUTABLE OFF)

qt_add_executable(TestLogModel
  tests/TestLogModel.cpp
  src/LogModel.cpp
  src/LogModel.h)
target_link_libraries(TestLogModel PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME TestLogModel COMMAND TestLogModel)
set_tests_properties(TestLogModel PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR}")

qt_add_executable(TestLogFilterProxy
  tests/TestLogFilterProxy.cpp
  src/LogFilterProxy.cpp
  src/LogFilterProxy.h
  src/LogModel.cpp
  src/LogModel.h)
target_link_libraries(TestLogFilterProxy PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME TestLogFilterProxy COMMAND TestLogFilterProxy)
set_tests_properties(TestLogFilterProxy PROPERTIES ENVIRONMENT "PATH=${QT_BIN_DIR}")
