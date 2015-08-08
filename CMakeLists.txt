project(SYSTEMD-KCM)
set(PROJECT_VERSION "1.2.1")

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
set(QT_MIN_VERSION "5.2.0")
set(KF5_MIN_VERSION "5.1.0")

# Silence a warning
cmake_policy(SET CMP0037 OLD)

find_package(ECM 1.1.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
             Widgets
             DBus)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
             Auth
             ConfigWidgets
             CoreAddons
             Crash
             I18n
             KIO
             WidgetsAddons)

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
# Adding local CMake modules
set (
   CMAKE_MODULE_PATH
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
   ${CMAKE_MODULE_PATH}
   )

configure_file(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

add_definitions(-DTRANSLATION_DOMAIN=\"systemd-kcm\")

add_subdirectory(other)
add_subdirectory(src)

ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
