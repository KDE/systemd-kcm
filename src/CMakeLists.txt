add_subdirectory(helper)

set(kcmsystemd_SRCS kcmsystemd.cpp
                    unitmodel.cpp
                    sortfilterunitmodel.cpp
                    confoption.cpp
                    confmodel.cpp
                    confdelegate.cpp
                    confparms.cpp
                    fsutil.cpp)

# Testing for C++0x/C++11 features
include (CheckCxxFeatures)
cxx_check_feature ("c++11" "std-chrono" "none" HAVE_CXX11_CHRONO "${ADDITIONAL_DEFINITIONS}")

# Fall back to boost::chrono if std::chrono feature detection failed
if (NOT HAVE_CXX11_CHRONO)
  message (WARNING "Your C++ compiler does not support std::chrono properly, using boost::chrono instead.")
  find_package(Boost 1.45.0 COMPONENTS chrono system REQUIRED)
endif ()

configure_file (kcm-systemd-features.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/kcm-systemd-features.h)

include_directories (${CMAKE_CURRENT_BINARY_DIR}/src)

ki18n_wrap_ui(kcmsystemd_SRCS ../ui/kcmsystemd.ui)

add_library(kcm_systemd MODULE ${kcmsystemd_SRCS})
target_link_libraries(kcm_systemd
                      Qt5::Widgets
                      Qt5::DBus
                      KF5::Auth
                      KF5::ConfigWidgets
                      KF5::CoreAddons
                      KF5::Crash
                      KF5::I18n
                      KF5::KIOCore
                      KF5::WidgetsAddons
                      ${Boost_LIBRARIES}
                      ${JOURNALD_LIBRARIES})
install(TARGETS kcm_systemd DESTINATION ${CMAKE_INSTALL_PLUGINDIR})
