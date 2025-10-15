#include "face_api.h"       // 1. 包含我们自己的头文件
#include <opencv2/opencv.hpp> // 2. 添加OpenCV头文件
#include "arcsoft_face_sdk.h"
#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include <iostream>  
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mutex> // 用于保证线程安全

using namespace std;

//从开发者中心获取APPID/SDKKEY(以下均为假数据，请替换)
#define APPID "Fse8387pw6RgBEQVRBSiD3MdHKZWTkoZmUDwvJszH2fy"
#define SDKKEY "GKoXEeo484fnHQgvrDH1kXiiq1Ckx7bhYPVCcymp4dH3"

// 其他宏定义
#define NSCALE 16       // 图像缩放系数，用于人脸检测
#define FACENUM	50       // 引擎最多能检测的人脸数量

// 全局唯一的引擎句柄，避免重复初始化
static MHandle g_faceEngineHandle = NULL;
// 用于保护引擎句柄初始化的互斥锁
static std::mutex g_engineMutex;

//时间戳转换为日期格式
void timestampToTime(char* timeStamp, char* dateTime, int dateTimeSize)
{
	time_t tTimeStamp = atoll(timeStamp);
	struct tm* pTm = gmtime(&tTimeStamp);
	strftime(dateTime, dateTimeSize, "%Y-%m-%d %H:%M:%S", pTm);
}

// C风格API函数的实现
// extern "C" 在头文件中已经声明，这里不需要重复

/**
 * @brief 初始化人脸识别引擎
 */
int InitFaceEngine()
{
    // 加锁，保证线程安全
    std::lock_guard<std::mutex> lock(g_engineMutex);

    // 如果已经初始化，则直接返回成功
    if (g_faceEngineHandle) {
        printf("Engine already initialized.\n");
        return MOK;
    }

	printf("\n************* Initializing ArcFace Engine *****************\n");

	// 在线激活SDK
	MRESULT res = ASFOnlineActivation(APPID, SDKKEY);
	if (MOK != res && MERR_ASF_ALREADY_ACTIVATED != res) 
    {
		printf("ASFOnlineActivation fail激活失败: %d\n", res);
        return res;
    }
	else
		printf("ASFOnlineActivation sucess激活成功: %d\n", res);

	//初始化引擎
	MInt32 mask = ASF_FACE_DETECT; // 我们只需要人脸检测功能来获取坐标
	res = ASFInitEngine(ASF_DETECT_MODE_IMAGE, ASF_OP_0_ONLY, NSCALE, FACENUM, mask, &g_faceEngineHandle);
	if (res != MOK)
	{
		printf("ASFInitEngine fail初始化引擎失败: %d\n", res);
        g_faceEngineHandle = NULL; // 初始化失败，确保句柄为空
		return res;
	}
	else
		printf("ASFInitEngine sucess初始化引擎成功: %d\n", res);

	printf("\n************* ArcFace SDK Info *****************\n");
	ASF_ActiveFileInfo activeFileInfo = { 0 };
	// 获取SDK激活文件信息，比如有效期
	res = ASFGetActiveFileInfo(&activeFileInfo);
	if (res != MOK)
	{
		printf("ASFGetActiveFileInfo fail: %d\n", res);
	}
	else
	{
		//这里仅获取了有效期时间，还需要其他信息直接打印即可
		char startDateTime[32];
		timestampToTime(activeFileInfo.startTime, startDateTime, 32);
		printf("startTime: %s\n", startDateTime);
		char endDateTime[32];
		timestampToTime(activeFileInfo.endTime, endDateTime, 32);
		printf("endTime: %s\n", endDateTime);
	}
	//SDK版本信息
	const ASF_VERSION version = ASFGetVersion();
	printf("\nVersion:%s\n", version.Version);
	printf("BuildDate:%s\n", version.BuildDate);
	printf("CopyRight:%s\n", version.CopyRight);

    return MOK;
}

// =========================================================================
// 内部辅助函数，处理所有核心检测逻辑，避免代码重复
// =========================================================================
static FaceRect* PerformFaceDetection(const cv::Mat& image, int* outFaceCount)
{
    // 检查输入图像是否有效
    if (image.empty()) {
        *outFaceCount = 0;
        return nullptr;
    }

    // 确保图像是3通道的
    if (image.channels() != 3) {
        printf("Error: Input image must be a 3-channel BGR image.\n");
        *outFaceCount = 0;
        return nullptr;
    }

    // 虹软SDK要求图像宽度为4的倍数
    int new_width = image.cols - (image.cols % 4);
    if (new_width == 0) {
        *outFaceCount = 0;
        return nullptr;
    }
    cv::Mat processed_mat = image(cv::Rect(0, 0, new_width, image.rows));

    // 3. 填充 ASVLOFFSCREEN 结构体，使用Ex接口
    ASVLOFFSCREEN offscreen = { 0 };
    offscreen.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8;
    offscreen.i32Width = processed_mat.cols;
    offscreen.i32Height = processed_mat.rows;
    offscreen.pi32Pitch[0] = (int)processed_mat.step;// 使用cv::Mat的step属性，它包含了内存对齐
    offscreen.ppu8Plane[0] = (MUInt8*)processed_mat.data;

    // 4. 进行人脸检测
    ASF_MultiFaceInfo detectedFaces = { 0 };
    MRESULT res = ASFDetectFacesEx(g_faceEngineHandle, &offscreen, &detectedFaces);
    if (res != MOK || detectedFaces.faceNum == 0) {
        *outFaceCount = 0;
        return nullptr;
    }

    // 5. 根据检测到的数量动态分配内存
    FaceRect* results = (FaceRect*)malloc(detectedFaces.faceNum * sizeof(FaceRect));
    if (!results) {
        *outFaceCount = 0;
        return nullptr;
    }

    // 6. 拷贝坐标数据
    for (int i = 0; i < detectedFaces.faceNum; ++i) {
        results[i].left = detectedFaces.faceRect[i].left;
        results[i].top = detectedFaces.faceRect[i].top;
        results[i].right = detectedFaces.faceRect[i].right;
        results[i].bottom = detectedFaces.faceRect[i].bottom;
    }

    *outFaceCount = detectedFaces.faceNum;
    return results;
}

/**
 * @brief 对传入的BGR图像数据进行人脸检测，动态分配内存
 */
FaceRect* DetectFacesDynamic(unsigned char* bgrImageData, int width, int height, int* outFaceCount)
{
    if (!g_faceEngineHandle || !bgrImageData || width <= 0 || height <= 0 || !outFaceCount) {
        if (outFaceCount) *outFaceCount = 0;
        return nullptr;
    }

    // 1. 将传入的裸数据包装成 cv::Mat，这是现代、安全的方式
    cv::Mat mat(height, width, CV_8UC3, bgrImageData);
    if (mat.empty()) {
        *outFaceCount = 0;
        return nullptr;
    }

    // 2. 调用通用的核心处理函数
    return PerformFaceDetection(mat, outFaceCount);
}

/**
 * @brief 通过文件路径加载图片并检测
 */
FaceRect* DetectFacesDynamicFromFile(const char* imagePath, int* outFaceCount)
{
    if (!g_faceEngineHandle || !imagePath || !outFaceCount) {
        if (outFaceCount) *outFaceCount = 0;
        return nullptr;
    }

    // 1. 使用OpenCV从文件加载图像
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        printf("Error: Failed to load image from path: %s\n", imagePath);
        *outFaceCount = 0;
        return nullptr;
    }

    // 2. 调用通用的核心处理函数
    return PerformFaceDetection(image, outFaceCount);
}


/**
 * @brief 释放内存
 */
void FreeFaceData(FaceRect* faceData)
{
    // 检查指针是否有效，然后释放
    if (faceData != nullptr)
    {
        free(faceData);
    }
}

/**
 * @brief 反初始化人脸识别引擎
 */
int UninitFaceEngine()
{
    // 加锁，保证线程安全
    std::lock_guard<std::mutex> lock(g_engineMutex);

    if (g_faceEngineHandle) {
        MRESULT res = ASFUninitEngine(g_faceEngineHandle);
        if (res != MOK) {
            printf("ASFUninitEngine fail: %d\n", res);
            return res;
        }
        g_faceEngineHandle = NULL;
        printf("Engine uninitialized successfully.\n");
    }
    return MOK;
}