#include <opencv2/opencv.hpp> // 1. 添加OpenCV头文件
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

using namespace std;

//从开发者中心获取APPID/SDKKEY(以下均为假数据，请替换)
#define APPID "Fse8387pw6RgBEQVRBSiD3MdHKZWTkoZmUDwvJszH2fy"
#define SDKKEY "GKoXEeo484fnHQgvrDH1kXiiq1Ckx7bhYPVCcymp4dH3"

// 其他宏定义
#define NSCALE 16       // 图像缩放系数，用于人脸检测
#define FACENUM	5       // 引擎最多能检测的人脸数量

// 安全释放内存的宏，避免野指针
#define SafeFree(p) { if ((p)) free(p); (p) = NULL; }
#define SafeArrayDelete(p) { if ((p)) delete [] (p); (p) = NULL; } 
#define SafeDelete(p) { if ((p)) delete (p); (p) = NULL; } 

//时间戳转换为日期格式
void timestampToTime(char* timeStamp, char* dateTime, int dateTimeSize)
{
	time_t tTimeStamp = atoll(timeStamp);
	struct tm* pTm = gmtime(&tTimeStamp);
	strftime(dateTime, dateTimeSize, "%Y-%m-%d %H:%M:%S", pTm);
}

//图像颜色格式转换
int ColorSpaceConversion(MInt32 width, MInt32 height, MInt32 format, MUInt8* imgData, ASVLOFFSCREEN& offscreen)
{
	offscreen.u32PixelArrayFormat = (unsigned int)format;
	offscreen.i32Width = width;
	offscreen.i32Height = height;
	
	switch (offscreen.u32PixelArrayFormat)
	{
	case ASVL_PAF_RGB24_B8G8R8:
		offscreen.pi32Pitch[0] = offscreen.i32Width * 3;
		offscreen.ppu8Plane[0] = imgData;
		break;
	case ASVL_PAF_I420:
		offscreen.pi32Pitch[0] = width;
		offscreen.pi32Pitch[1] = width >> 1;
		offscreen.pi32Pitch[2] = width >> 1;
		offscreen.ppu8Plane[0] = imgData;
		offscreen.ppu8Plane[1] = offscreen.ppu8Plane[0] + offscreen.i32Height*offscreen.i32Width;
		offscreen.ppu8Plane[2] = offscreen.ppu8Plane[0] + offscreen.i32Height*offscreen.i32Width * 5 / 4;
		break;
	case ASVL_PAF_NV12:
	case ASVL_PAF_NV21:
		offscreen.pi32Pitch[0] = offscreen.i32Width;
		offscreen.pi32Pitch[1] = offscreen.pi32Pitch[0];
		offscreen.ppu8Plane[0] = imgData;
		offscreen.ppu8Plane[1] = offscreen.ppu8Plane[0] + offscreen.pi32Pitch[0] * offscreen.i32Height;
		break;
	case ASVL_PAF_YUYV:
	case ASVL_PAF_DEPTH_U16:
		offscreen.pi32Pitch[0] = offscreen.i32Width * 2;
		offscreen.ppu8Plane[0] = imgData;
		break;
	case ASVL_PAF_GRAY:
		offscreen.pi32Pitch[0] = offscreen.i32Width;
		offscreen.ppu8Plane[0] = imgData;
		break;
	default:
		return 0;
	}
	return 1;
}

// 修改后的函数，接收图像裸数据、宽度和高度
void FaceIdentify(unsigned char* data, int width, int height)
{
	// 检查传入的数据是否有效
	if (!data || width <= 0 || height <= 0)
	{
		printf("Input image data is invalid.\n");
		return;
	}

	printf("\n************* ArcFace SDK Info *****************\n");
	MRESULT res = MOK;
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

	printf("\n************* Face Recognition *****************\n");
	
	// 在线激活SDK
	res = ASFOnlineActivation(APPID, SDKKEY);
	if (MOK != res && MERR_ASF_ALREADY_ACTIVATED != res)
		printf("ASFOnlineActivation fail激活失败: %d\n", res);
	else
		printf("ASFOnlineActivation sucess激活成功: %d\n", res);

	//初始化引擎
	MHandle handle = NULL;
	// 修改：移除了 ASF_IR_LIVENESS，因为我们处理的是单张BGR图像
	MInt32 mask = ASF_FACE_DETECT | ASF_FACERECOGNITION | ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | ASF_LIVENESS;
	res = ASFInitEngine(ASF_DETECT_MODE_IMAGE, ASF_OP_0_ONLY, NSCALE, FACENUM, mask, &handle);
	if (res != MOK)
	{
		printf("ASFInitEngine fail初始化引擎失败: %d\n", res);
		return; // 初始化失败，直接返回
	}
	else
		printf("ASFInitEngine sucess初始化引擎成功: %d\n", res);
	
	/********* 修改：不再从文件中读取图片，而是使用传入的data *********/

	// 1. 新增：将传入的 unsigned char* data 包装成 cv::Mat 对象。
	//    我们假设传入的数据是 BGR 格式，这与 OpenCV 的 CV_8UC3 默认格式一致。
	cv::Mat mat(height, width, CV_8UC3, data);

	if (mat.empty())
	{
		printf("Failed to create Mat from input data.\n");
		ASFUninitEngine(handle); // 反初始化引擎
		return;
	}

	// 2. 新增：直接使用 cv::Mat 的属性来填充 ASVLOFFSCREEN 结构体
	ASVLOFFSCREEN offscreen = { 0 };
	offscreen.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8; // 虹软SDK中与BGR对应的格式
	offscreen.i32Width = mat.cols;
	offscreen.i32Height = mat.rows;
	offscreen.ppu8Plane[0] = mat.data; // 直接使用Mat的数据指针
	offscreen.pi32Pitch[0] = mat.step; // **关键**：使用Mat的step属性，它考虑了内存对齐

	// 3. 对这张图像进行人脸检测
	ASF_MultiFaceInfo detectedFaces = { 0 };
	res = ASFDetectFacesEx(handle, &offscreen, &detectedFaces);
	
	if (res != MOK || detectedFaces.faceNum == 0)
	{
		printf("ASFDetectFacesEx fail or no face detected, res_code: %d\n", res);
	}
	else
	{
		printf("ASFDetectFacesEx sucess, found %d faces.\n", detectedFaces.faceNum);

		// 提取第一张人脸的特征
		ASF_SingleFaceInfo singleFaceInfo = { 0 };
		singleFaceInfo.faceRect.left = detectedFaces.faceRect[0].left;
		singleFaceInfo.faceRect.top = detectedFaces.faceRect[0].top;
		singleFaceInfo.faceRect.right = detectedFaces.faceRect[0].right;
		singleFaceInfo.faceRect.bottom = detectedFaces.faceRect[0].bottom;
		singleFaceInfo.faceOrient = detectedFaces.faceOrient[0];
		
		ASF_FaceFeature feature = { 0 };
		// 单人脸特征提取
		res = ASFFaceFeatureExtractEx(handle, &offscreen, &singleFaceInfo, &feature);
		if (res != MOK)
		{
			printf("ASFFaceFeatureExtractEx fail: %d\n", res);
		}
		else
		{
			printf("Face feature extracted successfully. You can now save it or compare it.\n");
			// 注意：此时 feature.feature 指向的内存由SDK管理，
			// 如果需要持久化存储或在别处使用，需要像原代码一样拷贝出来。
			// MByte* feature_copy = (MByte*)malloc(feature.featureSize);
			// memcpy(feature_copy, feature.feature, feature.featureSize);
		}

		printf("\n************* Face Process *****************\n");
		//设置活体置信度 SDK内部默认值为 IR：0.7  RGB：0.5（无特殊需要，可以不设置）
		ASF_LivenessThreshold threshold = { 0 };
		threshold.thresholdmodel_BGR = 0.5;
		threshold.thresholdmodel_IR = 0.7; // 即使不使用IR，设置了也无妨
		res = ASFSetLivenessParam(handle, &threshold);
		if (res != MOK)
			printf("ASFSetLivenessParam fail: %d\n", res);
		else
			printf("RGB Threshold: %f\nIR Threshold: %f\n", threshold.thresholdmodel_BGR, threshold.thresholdmodel_IR);

		// 人脸信息检测
		MInt32 processMask = ASF_AGE | ASF_GENDER | ASF_FACE3DANGLE | ASF_LIVENESS;
		res = ASFProcessEx(handle, &offscreen, &detectedFaces, processMask);
		if (res != MOK)
			printf("ASFProcessEx fail: %d\n", res);
		else
			printf("ASFProcessEx sucess: %d\n", res);

		// 获取年龄
		ASF_AgeInfo ageInfo = { 0 };
		res = ASFGetAge(handle, &ageInfo);
		if (res != MOK)
			printf("ASFGetAge fail: %d\n", res);
		else
			// 遍历所有检测到的人脸年龄
			for (int i = 0; i < detectedFaces.faceNum; ++i)
				printf("Face %d age: %d\n", i + 1, ageInfo.ageArray[i]);

		// 获取性别
		ASF_GenderInfo genderInfo = { 0 };
		res = ASFGetGender(handle, &genderInfo);
		if (res != MOK)
			printf("ASFGetGender fail: %d\n", res);
		else
			for (int i = 0; i < detectedFaces.faceNum; ++i)
				printf("Face %d gender: %d (0:女, 1:男, -1:未知)\n", i + 1, genderInfo.genderArray[i]);

		// 获取3D角度
		ASF_Face3DAngle angleInfo = { 0 };
		res = ASFGetFace3DAngle(handle, &angleInfo);
		if (res != MOK)
			printf("ASFGetFace3DAngle fail: %d\n", res);
		else
			for (int i = 0; i < detectedFaces.faceNum; ++i)
				printf("Face %d 3dAngle: roll: %lf yaw: %lf pitch: %lf\n", i + 1, angleInfo.roll[i], angleInfo.yaw[i], angleInfo.pitch[i]);
		
		//获取活体信息
		ASF_LivenessInfo rgbLivenessInfo = { 0 };
		res = ASFGetLivenessScore(handle, &rgbLivenessInfo);
		if (res != MOK)
			printf("ASFGetLivenessScore fail: %d\n", res);
		else
			for (int i = 0; i < detectedFaces.faceNum; ++i)
				printf("Face %d Liveness: %d (1:活体, 0:非活体, -1:不确定)\n", i + 1, rgbLivenessInfo.isLive[i]);
	}
	
	//反初始化
	res = ASFUninitEngine(handle);
	if (res != MOK)
		printf("ASFUninitEngine fail: %d\n", res);
	else
		printf("ASFUninitEngine sucess: %d\n", res);

	// getchar(); // 如果是在循环或回调中调用此函数，建议移除getchar()，避免程序阻塞
}
int main()
{

    return 0;
}

