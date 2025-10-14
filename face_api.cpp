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

/**
 * @brief 对传入的BGR图像数据进行人脸检测
 */
FaceRect* DetectFacesDynamic(unsigned char* bgrImageData, int width, int height, int* outFaceCount)
{
    // 检查引擎是否已初始化
    if (!g_faceEngineHandle) {
        printf("Engine not initialized. Please call InitFaceEngine first.\n");
        if (outFaceCount) *outFaceCount = 0; // 安全地设置输出参数
        return nullptr; // 返回空指针表示失败
    }

    // 检查传入的参数是否有效
    if (!bgrImageData || width <= 0 || height <= 0 || !outFaceCount) {
        printf("Input parameters are invalid.\n");
        if (outFaceCount) *outFaceCount = 0;
        return nullptr;
    }

	// 1. 将传入的 unsigned char* data 包装成 cv::Mat 对象。
	//    我们假设传入的数据是 BGR 格式，这与 OpenCV 的 CV_8UC3 默认格式一致。
	cv::Mat mat(height, width, CV_8UC3, bgrImageData);
	if (mat.empty())
	{
		printf("Failed to create Mat from input data.\n");
		*outFaceCount = 0;
		return nullptr;
	}

	// 2. 直接使用 cv::Mat 的属性来填充 ASVLOFFSCREEN 结构体
	ASVLOFFSCREEN offscreen = { 0 };
	offscreen.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8; // 虹软SDK中与BGR对应的格式
	offscreen.i32Width = mat.cols;
	offscreen.i32Height = mat.rows;
	offscreen.ppu8Plane[0] = mat.data; // 直接使用Mat的数据指针
	offscreen.pi32Pitch[0] = mat.step; // **关键**：使用Mat的step属性，它考虑了内存对齐

	// 3. 对这张图像进行人脸检测
	ASF_MultiFaceInfo detectedFaces = { 0 };
	MRESULT res = ASFDetectFacesEx(g_faceEngineHandle, &offscreen, &detectedFaces);
	
	if (res != MOK || detectedFaces.faceNum == 0)
	{
        // 如果检测失败或没有检测到人脸
		if (res != MOK)
        {
            printf("ASFDetectFacesEx fail, res_code: %d\n", res);
        }
        *outFaceCount = 0;
        return nullptr; // 返回空指针
	}
    
    // 4. 新增核心逻辑：根据检测到的人脸数量，动态分配内存
    // 使用 C-style 的 malloc，因为它与 free 配对，是跨语言 ABI 最稳定的方式
    FaceRect* result_buffer = (FaceRect*)malloc(detectedFaces.faceNum * sizeof(FaceRect));
    if (!result_buffer) 
    {
        // 内存分配失败是一种严重的运行时错误
        printf("Error: Failed to allocate memory for face results.\n");
        *outFaceCount = 0;
        return nullptr;
    }
    
    // 5. 将检测到的人脸坐标拷贝到新分配的内存中
    for (int i = 0; i < detectedFaces.faceNum; ++i) 
    {
        result_buffer[i].left   = detectedFaces.faceRect[i].left;
        result_buffer[i].top    = detectedFaces.faceRect[i].top;
        result_buffer[i].right  = detectedFaces.faceRect[i].right;
        result_buffer[i].bottom = detectedFaces.faceRect[i].bottom;
    }

    // 6. 通过输出参数返回人脸的实际数量
    *outFaceCount = detectedFaces.faceNum;

    // 7. 返回指向新分配内存的指针
    return result_buffer;
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