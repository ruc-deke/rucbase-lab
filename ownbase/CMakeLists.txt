cmake_minimum_required(VERSION 3.16)
project(YourBase)

link_directories(./lib)

add_executable(yourbase main.cpp)
target_link_libraries(yourbase ownbase parser execution system index record transaction recovery storage rwlatch readline pthread)
