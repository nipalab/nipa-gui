cmake_minimum_required(VERSION 3.24)
project(nipa-gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
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

add_executable(nipa-gui WIN32
    src/main.cpp
    src/daemonconnection.cpp
    src/daemonconnection.h
    src/mainwindow.cpp
    src/mainwindow.h
)
target_link_libraries(nipa-gui PRIVATE Qt6::Widgets protobuf::libprotobuf ${NIPA_GRPC_LIBS})
target_include_directories(nipa-gui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/proto"
)

protobuf_generate(
    TARGET nipa-gui
    LANGUAGE cpp
    PROTOS proto/server.proto proto/daemon.proto
)
protobuf_generate(
    TARGET nipa-gui
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${NIPA_GRPC_PLUGIN}"
    PROTOS proto/server.proto proto/daemon.proto
)
