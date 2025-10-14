cmake_minimum_required(VERSION 3.0)		#指定cmake版本 
project(FaceApi_Linux)				#指定项目的名称，一般和项目的文件夹名称对应

set(CMAKE_CXX_STANDARD 11)				#设置c++标准

# 查找线程库 (pthreads)
find_package(Threads REQUIRED)
# 查找OpenCV
find_package(OpenCV 4.9.0 REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "OpenCV version: ${OpenCV_VERSION}")
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

#指定头文件目录
include_directories(./)	
# 将OpenCV的头文件目录也包含进来
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(./inc)

#指定静态和动态文件目录
link_directories(./linux_so)

#设置opencv动态库位置
set(OpenCV_LIBS 
    "/usr/local/lib/libopencv_world.so.4.9.0"
)

# --- 核心修改：从生成可执行文件改为生成动态库 ---
# 2. 修改：使用 add_library 代替 add_executable
#    - "face_api" 是你库的名字，最终会生成 libface_api.so
#    - "SHARED" 指定了我们要生成的是动态库 (.so)
#    - "face_api.cpp" 是生成这个库的源文件 (请确保这个文件在您的项目根目录)
add_library(face_api SHARED
        face_api.cpp)

# 3. 修改：设置属性的目标从旧的 arcsoft_face_engine_test 改为新的 face_api
#    -fPIC 选项对于生成动态库是必需的，它让代码变成位置无关码
set_property(TARGET face_api PROPERTY POSITION_INDEPENDENT_CODE ON)

# 4. 修改：链接库文件的目标和内容
#    - 链接目标现在是新的库 "face_api"
#    - 添加了必须的链接项：
#      - ${OpenCV_LIBS}: 这是 find_package(OpenCV) 找到的所有OpenCV库
#      - Threads::Threads: 因为 face_api.cpp 中使用了 std::mutex，在Linux下需要链接pthread库
target_link_libraries(face_api
		arcsoft_face
		arcsoft_face_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libopencv_world.so.409
        Threads::Threads
        )

# ============================================================================
# 设置 RPATH (运行时搜索路径)
# ============================================================================

# 设置最终安装后的 RPATH
# $ORIGIN 是一个特殊的链接器变量，代表“库文件自己所在的目录”
# 这使得我们的库无论被移动到哪里，都能在自己的旁边找到其他依赖
# 设置构建时的 RPATH
set_target_properties(face_api PROPERTIES
    BUILD_RPATH "$ORIGIN"
    #BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/libs"
)

# 5. 可选：设置库文件的输出路径
#    默认会生成在 build 目录下，如果想指定到比如项目的 lib 目录下，可以取消下面的注释
# set_target_properties(face_api PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# ===================================================================
# ===== 新增：编译一个用于测试API库的可执行文件 =====
# ===================================================================
# 定义可执行文件的名称为 api_test_client，源文件是 test_api.cpp
add_executable(api_test_client test_api.cpp)

# 告诉CMake，我们的测试程序(api_test_client)需要链接到
# 我们刚刚编译的 face_api 库以及OpenCV库
target_link_libraries(api_test_client
    face_api        # 链接到我们自己生成的 libfaceapi.so
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libopencv_world.so.409  # 链接到OpenCV
)