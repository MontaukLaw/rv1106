cmake_minimum_required(VERSION 3.10)
project(yolo_rtsp)
message(STATUS "cmake version ${CMAKE_VERSION}")

set(TOOLCHAIN_DIR /home/marc/rv1106/toolchain/env/arm-rockchip830-linux-uclibcgnueabihf)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-rockchip830-linux-uclibcgnueabihf-g++)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc)
# /home/marc/rv1106/toolchain/env/arm-rockchip830-linux-uclibcgnueabihf/
# arm-rockchip830-linux-uclibcgnueabihf/sysroot
set(SYSROOT /home/marc/rv1106/sysroot)
set(MEDIA_ROOT /home/marc/rv1106/media/out)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${MEDIA_ROOT}/include)
include_directories(${MEDIA_ROOT}/include/rkaiq/uAPI2)
include_directories(${MEDIA_ROOT}/include/rkaiq/common)
include_directories(${MEDIA_ROOT}/include/rkaiq)
include_directories(${MEDIA_ROOT}/include/rkaiq/xcore)
include_directories(${MEDIA_ROOT}/include/rkaiq/algos)
include_directories(${MEDIA_ROOT}/include/rkaiq/iq_parser)
include_directories(${MEDIA_ROOT}/include/rkaiq/iq_parser_v2)
include_directories(/home/marc/rv1106/media/samples/example/common/)
set(CMAKE_CXX_STANDARD 11)
add_definitions(-DRKAIQ)

add_executable(app src/main.c src/sample_comm_isp.c 
                              src/sample_comm_vi.c 
                              src/sample_comm_venc.c
                              src/sample_comm.c) 
                   
# target_link_libraries(app rockchip_mpp m pthread rockit_full dl stdc++ rtsp rkaiq rga)
target_link_libraries(app rockchip_mpp m pthread rockit_full dl stdc++ rtsp rkaiq rga)
target_include_directories(app PUBLIC "${MEDIA_ROOT}/lib" ${CMAKE_CURRENT_SOURCE_DIR}/libs)

