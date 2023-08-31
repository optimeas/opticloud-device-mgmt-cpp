project(opticloud-device-mgmt-cpp)

set(HEADERS
	include/opticloud-device-mgmt-cpp/singleentrytransfer.hpp
        include/opticloud-device-mgmt-cpp/connectionparameters.hpp
)

set(SOURCES
	singleentrytransfer.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    libcurl-wrapper
)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
    PRIVATE .                 # "dot" is redundant, because local headers are always available in C/C++.
)
