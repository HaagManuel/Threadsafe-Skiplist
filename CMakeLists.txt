cmake_minimum_required (VERSION 2.8)

#### USER DEFINED ##############################################################

#### BASIC SETTINGS ############################################################

include_directories(.)

#set (CMAKE_CXX_FLAGS "-std=c++17 -msse4.2 -Wall -Wextra -O3 -g")
#set (CMAKE_CXX_FLAGS "-std=c++20 -msse4.2 -Wall -Wextra -O3 -g")
set (CMAKE_CXX_FLAGS "-std=c++2a  -msse4.2 -Wall -Wextra -O3 -g")

find_package(Threads REQUIRED)

find_package(OpenMP REQUIRED)

#### TARGETS ###################################################################

add_executable(test source/test.cpp)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(test PRIVATE Threads::Threads)

add_executable(time source/time.cpp)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(time PRIVATE Threads::Threads)
target_link_libraries(time PRIVATE OpenMP::OpenMP_CXX) #test