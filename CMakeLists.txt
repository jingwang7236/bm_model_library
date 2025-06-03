cmake_minimum_required(VERSION 3.10)
project(yolov8_bmcv)

set(CMAKE_BUILD_TYPE Debug)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
message(STATUS "cur dir: ${PROJECT_SOURCE_DIR}")

#Usage: check_and_replace_path(SDK)
#Input src path, output absolute path.
function(check_and_replace_path ARG_NAME) 
    if(IS_ABSOLUTE ${${ARG_NAME}})
        return()
    endif()
    set(PATH_TO_CHECK ${CMAKE_CURRENT_BINARY_DIR}/${${ARG_NAME}})
    if(EXISTS ${PATH_TO_CHECK})
        message(STATUS "Path ${PATH_TO_CHECK} exists")
        get_filename_component(ABSOLUTE_PATH ${PATH_TO_CHECK} ABSOLUTE)
        if(EXISTS ${ABSOLUTE_PATH})
            set(${ARG_NAME} ${ABSOLUTE_PATH} PARENT_SCOPE)
        else()
            message(FATAL_ERROR "Invalid path!")
        endif()
    else()
        message(FATAL_ERROR "Path ${PATH_TO_CHECK} does not exist")
    endif()
endfunction()

if (NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH pcie)
endif()

set(SDK "/data/workspace/soc-sdk")
message(STATUS "soc mode, starting......")
check_and_replace_path(${SDK})
message(STATUS "SDK: " ${SDK})

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a -std=c++11 -O2")
set(BM_LIBS bmlib bmrt bmcv yuv)
set(JPU_LIBS bmjpuapi bmjpulite)
set(OPENCV_LIBS opencv_imgproc opencv_core opencv_highgui opencv_imgcodecs opencv_videoio)
set(YAML_LIBS yaml-cpp)

include_directories("${SDK}/include/")
include_directories("${SDK}/include/opencv4")
include_directories("${SDK}/include/yaml-cpp")
link_directories("${SDK}/lib/")

# 添加静态库目标
add_subdirectory(
    ${CMAKE_SOURCE_DIR}/src
)

aux_source_directory(. SRC_FILES)
add_executable(yolov8_bmcv.soc ${SRC_FILES})
target_link_libraries(yolov8_bmcv.soc 
    bm_model_library
    dl
    ${YAML_LIBS}
    ${BM_LIBS} 
    ${OPENCV_LIBS} 
    -lpthread 
    -lavcodec 
    -lavformat 
    -lavutil
    )

set_target_properties(yolov8_bmcv.soc PROPERTIES
    LINK_FLAGS "-Wl,--allow-shlib-undefined"
)