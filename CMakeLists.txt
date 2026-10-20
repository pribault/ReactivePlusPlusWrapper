cmake_minimum_required(VERSION 3.25)
project(ReactivePlusPlusWrapper VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(RPPW_BUILD_EXAMPLES "Build the public API examples" OFF)

find_package(RPP CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(ReactivePlusPlusWrapper STATIC src/Completable.cpp src/Disposable.cpp)
add_library(ReactivePlusPlusWrapper::ReactivePlusPlusWrapper ALIAS ReactivePlusPlusWrapper)
target_compile_features(ReactivePlusPlusWrapper PUBLIC cxx_std_20)
target_include_directories(ReactivePlusPlusWrapper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(ReactivePlusPlusWrapper PUBLIC RPP::rpp Threads::Threads)
if(MSVC)
    target_compile_options(ReactivePlusPlusWrapper PUBLIC /Zc:__cplusplus /permissive-)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(RPPW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS ReactivePlusPlusWrapper EXPORT ReactivePlusPlusWrapperTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ReactivePlusPlusWrapperTargets
    NAMESPACE ReactivePlusPlusWrapper::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactivePlusPlusWrapper)
configure_package_config_file(cmake/ReactivePlusPlusWrapperConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ReactivePlusPlusWrapperConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactivePlusPlusWrapper)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ReactivePlusPlusWrapperConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ReactivePlusPlusWrapperConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ReactivePlusPlusWrapperConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ReactivePlusPlusWrapper)
install(FILES LICENSE THIRD_PARTY_NOTICES DESTINATION ${CMAKE_INSTALL_DATADIR}/ReactivePlusPlusWrapper)
