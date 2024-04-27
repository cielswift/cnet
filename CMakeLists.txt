 cmake_minimum_required(VERSION 3.6.2)

 project(linuxDev)

 #调试
 set(CMAKE_C_FLAGS_DEBUG "-g")
 set(CMAKE_CXX_FLAGS_DEBUG "-g")
 add_compile_options(-g)

 set(CMAKE_C_STANDARD 99)
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

 #可执行文件
 file(GLOB_RECURSE SRC_DIR_LIST "./app.cpp" "./the_epoll_net/*.cpp" "./the_epoll_net/*.h")
 add_executable(linuxDev ${SRC_DIR_LIST})
 #系统头文件
 include_directories(SYSTEM C:/software/project/cpp/cpp_learns/linux_dev/include_linux)

 #target_include_directories(linuxDev PRIVATE C:/software/project/cpp/cpp_learns/linux_dev/include/linux)

 #线程
 target_link_libraries(linuxDev PRIVATE pthread)
 #动态库
 target_link_libraries(linuxDev PRIVATE -ldl)
