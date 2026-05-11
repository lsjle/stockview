cmake_minimum_required(VERSION 3.16)
project(StockView)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql Charts)

# Enable automatic MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Source files
set(SOURCES
    stockview.cpp
)

# Create executable
add_executable(stockview ${SOURCES})

# Link Qt libraries
target_link_libraries(stockview
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Charts
)

# Install target
install(TARGETS stockview DESTINATION bin)
