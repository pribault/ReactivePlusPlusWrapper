find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ReactivePlusPlusWrapperTests
    CompletableTests.cpp
    CompositionTests.cpp
    DisposableTests.cpp
    FallbackTests.cpp
    MaybeTests.cpp
    ObservableTests.cpp
    SchedulerTests.cpp
    SideEffectTests.cpp
    SingleTests.cpp
    TimingTests.cpp
    TypesTests.cpp)
target_include_directories(ReactivePlusPlusWrapperTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(ReactivePlusPlusWrapperTests PRIVATE /bigobj)
endif()
set(test_main GTest::gtest_main)
get_target_property(gmock_library_type GTest::gmock TYPE)
if(WIN32 AND gmock_library_type STREQUAL "SHARED_LIBRARY")
    # Keep main and registration in the same registry of the amalgamated Windows DLL.
    set(test_main GTest::gmock_main)
endif()
target_link_libraries(ReactivePlusPlusWrapperTests PRIVATE
    ReactivePlusPlusWrapper::ReactivePlusPlusWrapper
    ${test_main} GTest::gmock)
gtest_discover_tests(ReactivePlusPlusWrapperTests PROPERTIES LABELS "unit")

add_executable(ReactivePlusPlusWrapperIntegrationTests SchedulerIntegrationTests.cpp)
target_link_libraries(ReactivePlusPlusWrapperIntegrationTests PRIVATE
    ReactivePlusPlusWrapper::ReactivePlusPlusWrapper GTest::gtest_main)
gtest_discover_tests(ReactivePlusPlusWrapperIntegrationTests
    PROPERTIES LABELS "integration" TIMEOUT 30)

set(header_checks)
foreach(header IN ITEMS Completable Concepts Disposable Maybe Observable ReactivePlusPlusWrapper Single)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/Standalone${header}.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "// rppw\n#include <rppw/${header}.h>\n")
    list(APPEND header_checks "${source}")
endforeach()
add_library(RppwStandaloneHeaders OBJECT ${header_checks})
target_link_libraries(RppwStandaloneHeaders PRIVATE
    ReactivePlusPlusWrapper::ReactivePlusPlusWrapper)
add_dependencies(ReactivePlusPlusWrapperTests RppwStandaloneHeaders)

add_executable(RppwMultiTuConsumer
    consumer/ConsumerMain.cpp
    consumer/ConsumerObservable.cpp
    consumer/ConsumerSingle.cpp)
target_include_directories(RppwMultiTuConsumer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/consumer)
target_link_libraries(RppwMultiTuConsumer PRIVATE
    ReactivePlusPlusWrapper::ReactivePlusPlusWrapper)
add_test(NAME PublicApi.MultiTranslationUnitConsumer COMMAND RppwMultiTuConsumer)
set_tests_properties(PublicApi.MultiTranslationUnitConsumer PROPERTIES LABELS "unit;compile")
