#ifndef FACE_API_H
#define FACE_API_H

// 定义跨平台导出宏
// 在Windows上编译时，需要定义 PRE_BUILD_DLL 宏，导出为 __declspec(dllexport)
// 在Linux/macOS上编译时，使用 GCC/Clang 的 visibility 属性
#if defined(_WIN32)
    #ifdef PRE_BUILD_DLL
        #define API_EXPORT __declspec(dllexport)
    #else
        #define API_EXPORT __declspec(dllimport)
    #endif
#else
    #define API_EXPORT __attribute__((visibility("default")))
#endif

// 定义一个结构体来存储人脸坐标
struct FaceRect {
    int left;
    int top;
    int right;
    int bottom;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化人脸识别引擎
 * 
 * @details 该函数应该在程序启动时调用一次。它会激活SDK并初始化引擎。
 * @return int 0代表成功，其他值代表失败的错误码。
 */
API_EXPORT int InitFaceEngine();

/**
 * @brief 对传入的BGR图像数据进行人脸检测，并由库动态分配内存返回结果。
 * 
 * @param bgrImageData 指向图像数据的指针。图像格式必须是24位BGR。
 * @param width 图像的宽度。
 * @param height 图像的高度。
 * @param outFaceCount [out] 一个整型指针，用于接收检测到的人脸数量。
 * @return FaceRect* 返回一个指向FaceRect数组的指针。如果未检测到人脸，返回nullptr。
 * @warning 返回的指针所指向的内存必须通过调用 FreeFaceData() 来释放，否则将导致内存泄漏！
 */
API_EXPORT FaceRect* DetectFacesDynamic(unsigned char* bgrImageData, int width, int height, int* outFaceCount);

/**
 * @brief 释放由 DetectFacesDynamic 函数分配的内存。
 * @details 这个函数必须被调用以防止内存泄漏。它会安全地检查指针是否为null。
 * @param faceData DetectFacesDynamic函数返回的指针。
 */
API_EXPORT void FreeFaceData(FaceRect* faceData);

/**
 * @brief 反初始化人脸识别引擎
 * 
 * @details 该函数应该在程序退出前调用一次，以释放SDK占用的所有资源。
 * @return int 0代表成功。
 */
API_EXPORT int UninitFaceEngine();


#ifdef __cplusplus
}
#endif

#endif // FACE_API_H