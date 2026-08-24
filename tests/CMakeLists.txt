qt_add_executable(tst_smoke tst_smoke.cpp)
target_link_libraries(tst_smoke PRIVATE Qt6::Test)
add_test(NAME smoke COMMAND tst_smoke)

add_executable(tst_generator tst_generator.cpp)
target_include_directories(tst_generator PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(tst_generator PRIVATE core Qt6::Test)
add_test(NAME generator COMMAND tst_generator)

add_executable(tst_json tst_json.cpp)
target_include_directories(tst_json PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(tst_json PRIVATE core Qt6::Test)
add_test(NAME json COMMAND tst_json)

add_executable(tst_srt tst_srt.cpp)
target_include_directories(tst_srt PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(tst_srt PRIVATE core Qt6::Test)
target_compile_definitions(tst_srt PRIVATE OPENBINGO_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
add_test(NAME srt COMMAND tst_srt)

add_executable(tst_csv_project tst_csv_project.cpp)
target_include_directories(tst_csv_project PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(tst_csv_project PRIVATE core Qt6::Test)
add_test(NAME csv_project COMMAND tst_csv_project)

# Live OpenSubtitles (search only). Skip si OPENSUBTITLES_API_KEY vide.
# ctest -L opensubtitles
qt_add_executable(tst_opensubtitles_live tst_opensubtitles_live.cpp)
target_link_libraries(tst_opensubtitles_live PRIVATE Qt6::Test Qt6::Network)
add_test(NAME opensubtitles_live COMMAND tst_opensubtitles_live)
set_tests_properties(opensubtitles_live PROPERTIES LABELS "opensubtitles")

qt_add_executable(tst_database tst_database.cpp)
target_link_libraries(tst_database PRIVATE store Qt6::Test Qt6::Sql)
add_test(NAME database COMMAND tst_database)

qt_add_executable(tst_appflow tst_appflow.cpp)
target_link_libraries(tst_appflow PRIVATE
    applib cryptolayer net store qrcodegen
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Sql Qt6::WebSockets Qt6::PrintSupport Qt6::Widgets Qt6::Test
    PkgConfig::SODIUM PkgConfig::SECP256K1
)
add_test(NAME appflow COMMAND tst_appflow)

qt_add_executable(tst_projectsync tst_projectsync.cpp)
target_link_libraries(tst_projectsync PRIVATE
    applib cryptolayer net store
    Qt6::Core Qt6::Gui Qt6::Sql Qt6::WebSockets Qt6::Test
    PkgConfig::SODIUM PkgConfig::SECP256K1
)
add_test(NAME projectsync COMMAND tst_projectsync)

add_test(NAME qml_smoke COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/smoke-ui.sh)
add_test(NAME live_xvfb COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/run-live-test.sh)
add_test(NAME resolutions COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/test-resolutions.sh)
add_test(NAME fullscreen_smoke COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/smoke-fullscreen-multi.sh)
