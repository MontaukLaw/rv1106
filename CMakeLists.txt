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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/rga)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/test)
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

add_executable(app src/main.cc src/postprocess.cc 
                              src/rknn_detect.cc
                              src/samples/sample_comm_isp.c 
                              src/samples/sample_comm_vi.c 
                              src/samples/sample_comm_venc.c
                              src/samples/sample_comm.c
                              src/utils.cc ) 

set(LIB_PATH ${CMAKE_SOURCE_DIR}/libs)
add_library(test_lib STATIC IMPORTED)
set_target_properties(test_lib PROPERTIES IMPORTED_LOCATION ${LIB_PATH}/librt_test_comm.a)

# target_link_libraries(app rockchip_mpp m pthread rockit_full dl stdc++ rtsp rkaiq rga)
target_link_libraries(app rockchip_mpp m pthread rockit_full dl stdc++ rtsp rkaiq rga rknnmrt)
target_include_directories(app PUBLIC "${MEDIA_ROOT}/lib" ${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_executable(test_vpss src/vpss_demo.cc src/utils.cc)
target_link_libraries(test_vpss pthread rockit_full dl stdc++ rockchip_mpp rkaiq rga test_lib)

add_executable(vi_vpss_demo src/vi_vpss.c  
                            src/samples/sample_comm_vpss.c
                            src/samples/sample_comm_isp.c 
                            src/samples/sample_comm_vi.c 
                            src/samples/sample_comm_venc.c
                            src/samples/sample_comm.c)
target_link_libraries(vi_vpss_demo pthread rockit_full dl stdc++ rockchip_mpp rkaiq rga test_lib rtsp)

#####################################
add_executable(vi_2_vpss_venc_rtsp src/vi_2_vpss_venc_rtsp.c  
                            src/samples/sample_comm_vpss.c
                            src/samples/sample_comm_isp.c 
                            src/samples/sample_comm_vi.c 
                            src/samples/sample_comm_venc.c
                            src/samples/sample_comm_rgn.c
                            src/samples/sample_comm_vo.c
                            src/samples/fillimage.c
                            src/samples/loadbmp.c
                            src/samples/sample_comm.c)
target_link_libraries(vi_2_vpss_venc_rtsp pthread rockit_full dl stdc++ rockchip_mpp rkaiq rga test_lib rtsp)

add_executable(no_bind src/no_bind_process.c  
                            src/samples/sample_comm_vpss.c
                            src/samples/sample_comm_isp.c 
                            src/samples/sample_comm_vi.c 
                            src/samples/sample_comm_venc.c
                            src/samples/sample_comm_rgn.c
                            src/samples/sample_comm_vo.c
                            src/samples/fillimage.c
                            src/samples/loadbmp.c
                            src/samples/sample_comm.c)
target_link_libraries(no_bind pthread rockit_full dl stdc++ rockchip_mpp rkaiq rga test_lib rtsp)

