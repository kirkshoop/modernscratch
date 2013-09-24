# The name of our project is "HELLO". CMakeLists files in this project can 
 # refer to the root source directory of the project as ${MODERNSCRATCH_SOURCE_DIR} and 
 # to the root binary directory of the project as ${MODERNSCRATCH_BINARY_DIR}. 
cmake_minimum_required (VERSION 2.8) 
project (MODERNSCRATCH) 

set(Boost_ADDITIONAL_VERSIONS "1.53" "1.53.0" )

 # Recurse into the "ext" subdirectory. This does not actually 
 # cause another cmake executable to run. The same process will walk through 
 # the project's entire directory structure. 
add_subdirectory (ext) 

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTI_THREADED ON)
find_package(Boost REQUIRED) 

if (Boost_FOUND)
  message("Boost information:") 
  message("  Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}") 
  message("  Boost_LIBRARIES: ${Boost_LIBRARIES}") 
  message("  Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}") 

  include_directories (${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})

  set(BOOST_CLIENT_LIBS
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_CHRONO_LIBRARY})
endif(Boost_FOUND)

find_package( OpenSSL )
if (OPENSSL_FOUND)
    add_definitions(-DBOOST_NETWORK_ENABLE_HTTPS)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(OPENSSL_CLIENT_LIBS
      ${OPENSSL_LIBRARIES})
endif()


# Make sure the compiler can find include files from the libraries. 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/ext/Rx/Ix/CPP/src) 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/ext/Rx/Rx/CPP/src) 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/ext/kirkshoop/libraries) 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/ext/cpp-netlib) 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/rxwin32) 
include_directories (${MODERNSCRATCH_SOURCE_DIR}/rxanimate) 

# Make sure the linker can find the static libraries once they are built. 
#link_directories (${MODERNSCRATCH_BINARY_DIR}/ext) 

# Add executable called "modernscratch" that is built from the source files 
# "modernscratch.cpp". 
add_executable (modernscratch WIN32 modernscratch.cpp) 

if (MSVC)
  set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
#  set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ")
#  set ( CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS} " )
  add_definitions(-D_UNICODE -DUNICODE)
endif ()

add_dependencies(modernscratch cppnetlib-uri cppnetlib-client-connections)

target_link_libraries(modernscratch
    ${BOOST_CLIENT_LIBS}
    ${OPENSSL_CLIENT_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    cppnetlib-uri
    cppnetlib-client-connections)

# Link the executable to the Hello library. 
#target_link_libraries (helloDemo Hello) 
