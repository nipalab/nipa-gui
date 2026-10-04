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

add_executable(repositorytreemodel_test repositorytreemodel_test.cpp)
target_link_libraries(repositorytreemodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME repositorytreemodel_test COMMAND repositorytreemodel_test)

add_executable(commitlogmodel_test commitlogmodel_test.cpp)
target_link_libraries(commitlogmodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME commitlogmodel_test COMMAND commitlogmodel_test)

add_executable(locksmodel_test locksmodel_test.cpp)
target_link_libraries(locksmodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME locksmodel_test COMMAND locksmodel_test)

add_executable(branchmodel_test branchmodel_test.cpp)
target_link_libraries(branchmodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME branchmodel_test COMMAND branchmodel_test)

add_executable(revisiongraphmodel_test revisiongraphmodel_test.cpp)
target_link_libraries(revisiongraphmodel_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME revisiongraphmodel_test COMMAND revisiongraphmodel_test)

add_executable(mainwindow_test mainwindow_test.cpp)
target_link_libraries(mainwindow_test PRIVATE nipa-gui-core Qt6::Test)
add_test(NAME mainwindow_test COMMAND mainwindow_test)
