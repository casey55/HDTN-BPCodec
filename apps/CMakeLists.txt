cmake_minimum_required(VERSION 3.0)
project(bpcodec_perf)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

add_executable(encode-benchmark
    encode-benchmark/encoding.cpp
)

target_include_directories(encode-benchmark PUBLIC ../include)
target_link_libraries(encode-benchmark PUBLIC bpcodec)

add_executable(bundle-benchmark
    bundle-benchmark/bundle.cpp
)

target_include_directories(bundle-benchmark PUBLIC ../include)
target_link_libraries(bundle-benchmark PUBLIC bpcodec)

add_executable(bpgen
    bpgen/bpgen.cpp
) 

target_include_directories(bpgen PUBLIC ../include)
target_link_libraries(bpgen PUBLIC bpcodec)

add_executable(bpsink
    bpsink/bpsink.cpp
)

target_include_directories(bpsink PUBLIC ../include)
target_link_libraries(bpsink PUBLIC bpcodec)



