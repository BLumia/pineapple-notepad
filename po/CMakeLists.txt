find_package(KF6I18n REQUIRED)
include("${KF6I18n_DIR}/KF6I18nMacros.cmake")

ki18n_install("${CMAKE_CURRENT_SOURCE_DIR}")
