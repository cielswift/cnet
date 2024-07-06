 cmake_minimum_required(VERSION 3.6.2)
 project(cnet)

 #调试
 set(CMAKE_C_FLAGS_DEBUG "-g")
 set(CMAKE_CXX_FLAGS_DEBUG "-g")
 add_compile_options(-g)

 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

 #可执行文件
 file(GLOB_RECURSE SRC_DIR_LIST "./app.cpp"
         "./the_epoll_net/*.cpp" "./the_epoll_net/*.h"
         "./json/*.c" "./json/*.h"
 )
 add_executable(cnet ${SRC_DIR_LIST})

 #线程
 target_link_libraries(cnet PRIVATE pthread)
 #动态库
 target_link_libraries(cnet PRIVATE -ldl)
