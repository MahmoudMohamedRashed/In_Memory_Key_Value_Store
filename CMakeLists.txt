cmake_minimum_required(VERSION 3.16)

project(In_Memory_Key_Value_Store LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(In_Memory_Key_Value_Store main.cpp
    KV-Store/kvstore.h KV-Store/kvstore.cpp
    "Command Parser/commandparser.h" "Command Parser/commandparser.cpp"
    "Tcp Session/tcpsession.h" "Tcp Session/tcpsession.cpp"
    TcpServer/tcpserver.h TcpServer/tcpserver.cpp
    KvServer/kvserver.h KvServer/kvserver.cpp)

add_definitions(-DASIO_STANDALONE)

include_directories("/home/mahmoud-rashed/asio-1.38.2/include")

include(GNUInstallDirs)
install(TARGETS In_Memory_Key_Value_Store
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
