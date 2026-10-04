find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(daemonconnection_test daemonconnection_test.cpp)
target_link_libraries(daemonconnection_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME daemonconnection_test COMMAND daemonconnection_test)

add_executable(daemonchannel_test daemonchannel_test.cpp)
target_link_libraries(daemonchannel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME daemonchannel_test COMMAND daemonchannel_test)

add_executable(statusmodel_test statusmodel_test.cpp)
target_link_libraries(statusmodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME statusmodel_test COMMAND statusmodel_test)

add_executable(mainwindow_test mainwindow_test.cpp)
target_link_libraries(mainwindow_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME mainwindow_test COMMAND mainwindow_test)
