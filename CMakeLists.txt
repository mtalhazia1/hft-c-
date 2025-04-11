cmake_minimum_required(VERSION 3.10)
project(tetherCPlusPlus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    src/main.cpp
    src/Engine.cpp
    src/Client.cpp
)

# Add header files
set(HEADERS
    src/Engine.h
    src/Client.h
    src/Order.h
    src/Types.h
)

# Create executable
add_executable(tetherCPlusPlus ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(tetherCPlusPlus PRIVATE src) 