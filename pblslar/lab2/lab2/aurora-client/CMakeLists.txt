cmake_minimum_required(VERSION 3.10)

project(ru.mai.music_collection VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
find_package(Qt5 5.6 REQUIRED COMPONENTS Core Gui Network Qml Quick Sql)
pkg_check_modules(AuroraApp REQUIRED IMPORTED_TARGET auroraapp)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/albumcontroller.cpp
    src/albumcontroller.h
    src/appsettings.h
    src/persistentcookiejar.cpp
    src/persistentcookiejar.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    PkgConfig::AuroraApp
    Qt5::Core Qt5::Gui Qt5::Network Qt5::Qml Qt5::Quick Qt5::Sql
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY qml DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
install(FILES ${PROJECT_NAME}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

foreach(icon_size 86x86 108x108 128x128 172x172)
    install(FILES icons/${icon_size}/${PROJECT_NAME}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}/apps)
endforeach()
