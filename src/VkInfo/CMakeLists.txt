cmake_minimum_required(VERSION 3.16)

project(VkInfo LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui)
find_package(Vulkan REQUIRED)

if(MSVC)
  if (${QT_VERSION_MAJOR} EQUAL 5)
    add_compile_options("/source-charset:utf-8")
    add_compile_options("/execution-charset:utf-8")
  endif()
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeGen")
endif()
add_executable(VkInfo
  main.cpp
)
target_link_libraries(VkInfo PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Vulkan::Vulkan
)

include(GNUInstallDirs)
install(TARGETS VkInfo
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
