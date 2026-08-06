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

qt_add_executable(tst_database tst_database.cpp)
target_link_libraries(tst_database PRIVATE store Qt6::Test Qt6::Sql)
add_test(NAME database COMMAND tst_database)
