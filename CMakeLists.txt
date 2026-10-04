cmake_minimum_required(VERSION 3.24)
project(nipa-gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(NIPA_BUILD_TESTS "Build the unit tests" OFF)
option(NIPA_COVERAGE "Instrument the sources for gcov coverage" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

find_package(gRPC CONFIG QUIET)
set(NIPA_GRPC_LIBS "")
if(TARGET gRPC::grpc++)
    set(NIPA_GRPC_LIBS gRPC::grpc++)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GRPCPP REQUIRED IMPORTED_TARGET grpc++)
    set(NIPA_GRPC_LIBS PkgConfig::GRPCPP)
endif()

# The codegen plugin ships separately from the libraries on Debian/Ubuntu and
# the CMake package does not always point at it.
set(NIPA_GRPC_PLUGIN "")
if(TARGET gRPC::grpc_cpp_plugin)
    set(NIPA_GRPC_PLUGIN "$<TARGET_FILE:gRPC::grpc_cpp_plugin>")
elseif(DEFINED gRPC_CPP_PLUGIN_EXECUTABLE AND gRPC_CPP_PLUGIN_EXECUTABLE)
    set(NIPA_GRPC_PLUGIN "${gRPC_CPP_PLUGIN_EXECUTABLE}")
else()
    find_program(NIPA_GRPC_PLUGIN NAMES grpc_cpp_plugin REQUIRED)
endif()

# Application logic lives in a library so that the unit tests can link it.
add_library(nipa-gui-core STATIC
    src/daemon/daemonchannel.cpp
    src/daemon/daemonchannel.h
    src/daemon/daemonconnection.cpp
    src/daemon/daemonconnection.h
    src/daemon/daemondiff.cpp
    src/daemon/daemondiff.h
    src/daemon/daemonoperation.cpp
    src/daemon/daemonoperation.h
    src/daemon/daemontypes.h
    src/models/branchmodel.cpp
    src/models/branchmodel.h
    src/models/commitlogmodel.cpp
    src/models/commitlogmodel.h
    src/models/locksmodel.cpp
    src/models/locksmodel.h
    src/models/repositorytreemodel.cpp
    src/models/repositorytreemodel.h
    src/models/revisiongraphmodel.cpp
    src/models/revisiongraphmodel.h
    src/models/statusmodel.cpp
    src/models/statusmodel.h
    src/services/branchservice.cpp
    src/services/branchservice.h
    src/services/graphservice.cpp
    src/services/graphservice.h
    src/services/historyservice.cpp
    src/services/historyservice.h
    src/services/lockservice.cpp
    src/services/lockservice.h
    src/services/repositoryservice.cpp
    src/services/repositoryservice.h
    src/services/statusservice.cpp
    src/services/statusservice.h
    src/ui/diffview.cpp
    src/ui/diffview.h
    src/ui/branchdialogs.cpp
    src/ui/branchdialogs.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/operationprogressdialog.cpp
    src/ui/operationprogressdialog.h
    src/ui/submitdialog.cpp
    src/ui/submitdialog.h
)
target_link_libraries(nipa-gui-core PUBLIC Qt6::Widgets Qt6::Concurrent protobuf::libprotobuf ${NIPA_GRPC_LIBS})
target_include_directories(nipa-gui-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/proto"
)

protobuf_generate(
    TARGET nipa-gui-core
    LANGUAGE cpp
    PROTOS proto/server.proto proto/daemon.proto
)
protobuf_generate(
    TARGET nipa-gui-core
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${NIPA_GRPC_PLUGIN}"
    PROTOS proto/server.proto proto/daemon.proto
)

add_executable(nipa-gui WIN32 src/main.cpp)
target_link_libraries(nipa-gui PRIVATE nipa-gui-core)

if(NIPA_COVERAGE)
    target_compile_options(nipa-gui-core PUBLIC --coverage -O0 -g)
    target_link_options(nipa-gui-core PUBLIC --coverage)
endif()

if(NIPA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
