cmake_minimum_required(VERSION 3.20)
project(ktorrentsearch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QT_MIN_VERSION "6.7.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    QuickControls2
    Network
    Test
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    KIO
)

add_library(ktorrentsearch_core STATIC
    src/torrentmodel.h
    src/torrentmodel.cpp
    src/torrentsearchbackend.h
    src/torrentsearchbackend.cpp
)

target_include_directories(ktorrentsearch_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ktorrentsearch_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    KF6::CoreAddons
    KF6::I18n
    KF6::KIOGui
)

add_executable(ktorrentsearch
    src/main.cpp
    src/resources.qrc
)

target_link_libraries(ktorrentsearch PRIVATE
    ktorrentsearch_core
)

install(TARGETS ktorrentsearch ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(DIRECTORY data/nova3 DESTINATION ${KDE_INSTALL_DATADIR}/ktorrentsearch)
install(FILES data/org.cesar.ktorrentsearch.desktop DESTINATION ${KDE_INSTALL_APPDIR})

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
