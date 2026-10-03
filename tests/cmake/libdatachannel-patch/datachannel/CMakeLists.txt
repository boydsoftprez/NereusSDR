# no-port-check: NereusSDR-original. See ../CMakeLists.txt.
# Custom targets: they carry SOURCES as libraries do and need no compiler.
add_custom_target(datachannel SOURCES src/peerconnection.cpp src/impl/dtlstransport.cpp)
add_custom_target(datachannel-static SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/peerconnection.cpp
                  ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/dtlstransport.cpp)
