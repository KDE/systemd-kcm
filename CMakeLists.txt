project(SYSTEMD-KCM)
set(PROJECT_VERSION "1.1.0")

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
set(QT_MIN_VERSION "5.0.0")
set(KF5_MIN_VERSION "5.0.0")

# Silence a warning
cmake_policy(SET CMP0037 OLD)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
             Widgets
             DBus)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
             CoreAddons
             WidgetsAddons
             ConfigWidgets
             I18n
             Service
             Auth)

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(FeatureSummary)
include(CheckIncludeFiles)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD "systemd" REQUIRED)

if(SYSTEMD_FOUND)
  # libsystemd-journal was merged into libsystemd in 209
  if(${SYSTEMD_VERSION} VERSION_LESS 209)
    pkg_check_modules(JOURNALD "libsystemd-journal" REQUIRED)
  else()
    pkg_check_modules(JOURNALD "libsystemd" REQUIRED)
  endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

add_definitions(-DTRANSLATION_DOMAIN=\"kcmsystemd\")

add_subdirectory(other)
add_subdirectory(src)

find_package(KF5I18n CONFIG REQUIRED)
ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
