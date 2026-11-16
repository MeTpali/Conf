add_executable(integration_client
    integration_client.cpp
    ../src/temperaturecontroller.cpp
    ../src/temperaturecontroller.h
)

target_include_directories(integration_client PRIVATE ../src)
target_link_libraries(integration_client PRIVATE Qt5::Core Qt5::Network)

