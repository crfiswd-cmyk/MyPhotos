cmake_minimum_required(VERSION 3.21)
project(MyPhotosCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ENABLE_VIPS "Use libvips for decoding/scaling when available" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Gui Concurrent Qml QuickControls2)

if(ENABLE_VIPS)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(VIPS QUIET vips-cpp)
    if(VIPS_FOUND)
      message(STATUS "Found libvips: ${VIPS_VERSION}")
      add_compile_definitions(HAVE_VIPS=1)
    endif()
  endif()
endif()

set(SOURCES
  src/main.cpp
  src/ImageDecoder.cpp
  src/ThumbCache.cpp
  src/ThumbProvider.cpp
  src/ImageListModel.cpp
  src/ThumbBridge.cpp
  src/SequentialFileReader.cpp
  src/TileCache.cpp
  src/TileLoader.cpp
  src/TiledImageItem.cpp
)

qt_add_resources(APP_RESOURCES qml/qml.qrc)

add_executable(MyPhotosCpp
  ${SOURCES}
  ${APP_RESOURCES}
)

target_include_directories(MyPhotosCpp PRIVATE src)

if(VIPS_FOUND)
  target_include_directories(MyPhotosCpp PRIVATE ${VIPS_INCLUDE_DIRS})
  target_link_directories(MyPhotosCpp PRIVATE ${VIPS_LIBRARY_DIRS})
  target_link_libraries(MyPhotosCpp PRIVATE ${VIPS_LIBRARIES})
endif()

target_link_libraries(MyPhotosCpp PRIVATE
  Qt6::Quick
  Qt6::Gui
  Qt6::Concurrent
  Qt6::Qml
  Qt6::QuickControls2
)

# On macOS, bundle the app for drag-and-drop running
if(APPLE)
  set_target_properties(MyPhotosCpp PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.myphotos"
    MACOSX_BUNDLE_BUNDLE_NAME "MyPhotos"
  )
endif()
