find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(daemonconnection_test daemonconnection_test.cpp)
target_link_libraries(daemonconnection_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME daemonconnection_test COMMAND daemonconnection_test)

add_executable(mainwindow_test mainwindow_test.cpp)
target_link_libraries(mainwindow_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME mainwindow_test COMMAND mainwindow_test)
