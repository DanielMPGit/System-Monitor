cmake_minimum_required(VERSION 3.14)
project(system_monitor)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG main
)

FetchContent_MakeAvailable(ftxui)

set(PAHO_WITH_SSL OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  paho_mqtt_cpp
  GIT_REPOSITORY https://github.com/eclipse-paho/paho.mqtt.cpp.git
  GIT_TAG master
)

FetchContent_MakeAvailable(paho_mqtt_cpp)

add_executable(system_monitor main.cpp)

target_link_libraries(system_monitor
  PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

add_executable(mqtt mqtt.cpp)

target_link_libraries(mqtt
  PRIVATE
    paho-mqttpp3-static
)
