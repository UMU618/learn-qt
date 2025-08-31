cmake_minimum_required(VERSION 3.16)

project(PcmPlayer LANGUAGES CXX)

# Qt5 only

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core LinguistTools Multimedia)
set(LIBS
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Multimedia
)

set(SRC_FILES
  src/main.cpp
)

set(TS_FILES
  res/PcmPlayer_en_US.ts
  res/PcmPlayer_zh_CN.ts
)
set_source_files_properties(${TS_FILES} PROPERTIES
  OUTPUT_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/i18n
  TARGET_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/i18n
)

if(MSVC)
  add_compile_options("/source-charset:utf-8")
  add_compile_options("/execution-charset:utf-8")
endif()
add_executable(PcmPlayer
  ${SRC_FILES}
  ${TS_FILES}
)
target_link_libraries(PcmPlayer ${LIBS})

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
add_custom_target(UpdateTranslation ALL DEPENDS ${QM_FILES})

include(GNUInstallDirs)
install(TARGETS PcmPlayer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
