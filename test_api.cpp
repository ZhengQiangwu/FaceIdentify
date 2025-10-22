#include <iostream>
#include <vector>
#include <string>

// 1. 包含OpenCV头文件，用于加载和显示图片
#include <opencv2/opencv.hpp>

// 2. 包含我们自己库的API头文件
//    假设这个头文件在 ./inc 目录下
#include "inc/face_api.h" 
// --- 新增：添加这两个头文件 ---
#include <unistd.h>  // 为了 getcwd
#include <limits.h>  // 为了 PATH_MAX

void PrintCurrentWorkingDirectory() {
    char current_path[PATH_MAX];
    if (getcwd(current_path, sizeof(current_path)) != NULL) {
        std::cout << "==> 当前工作目录是: " << current_path << std::endl;
    } else {
        perror("getcwd() error");
    }
}

int main(int argc, char* argv[])
{
    // 检查是否提供了图片路径作为命令行参数
    if (argc < 2)
    {
        std::cout << "请提供一个图片文件的路径作为参数！" << std::endl;
        std::cout << "用法: ./api_test_client <图片路径>" << std::endl;
        return -1;
    }
    std::string imagePath = argv[1];

    std::cout << "程序启动..." << std::endl;
    PrintCurrentWorkingDirectory();

    // --- 步骤 2: 初始化引擎 ---
    std::cout << "正在调用 InitFaceEngine()..." << std::endl;
    int ret = InitFaceEngine();
    if (ret != 0)
    {
        std::cerr << "引擎初始化失败，错误码: " << ret << std::endl;
        return -1;
    }
    std::cout << "引擎初始化成功！" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    PrintCurrentWorkingDirectory();
    // --- 步骤 2: 加载要测试的图片 ---
    cv::Mat image = cv::imread(imagePath);
    if (image.empty())
    {
        std::cerr << "无法加载图片: " << imagePath << std::endl;
        // 即使图片加载失败，也要反初始化引擎
        UninitFaceEngine();
        return -1;
    }
    
    // 确保图片是3通道的BGR图像
    if (image.channels() != 3) {
        std::cerr << "输入图片必须是3通道的BGR图像。" << std::endl;
        UninitFaceEngine();
        return -1;
    }
    
    std::cout << "图片加载成功: " << image.cols << "x" << image.rows << std::endl;

    // --- 步骤 3 & 4: 调用新的动态检测函数 ---
    std::cout << "正在调用 DetectFacesDynamic()..." << std::endl;
    int faceCount = 0;
    FaceRect* faceResults = nullptr;
    //调用核心检测函数
    //image.data 是指向BGR图像裸数据的指针
    faceResults = DetectFacesDynamic(image.data, image.cols, image.rows, &faceCount);

    // --- 步骤 5: 处理并显示结果 ---
    if (faceResults != nullptr && faceCount > 0)
    {
        std::cout << "成功检测到 " << faceCount << " 个人脸！" << std::endl;

        // 遍历检测到的每一个脸，打印坐标并在图上画框
        for (int i = 0; i < faceCount; ++i)
        {
            FaceRect rect = faceResults[i];
            std::cout << "  人脸 " << i + 1 << ": "
                      << "Left=" << rect.left << ", Top=" << rect.top 
                      << ", Right=" << rect.right << ", Bottom=" << rect.bottom << std::endl;

            // 使用OpenCV在图片上画出矩形框
            // 参数: 图片, 左上角点, 右下角点, 颜色(BGR格式，这里是绿色), 线条宽度
            cv::rectangle(image, cv::Point(rect.left, rect.top), cv::Point(rect.right, rect.bottom), cv::Scalar(0, 255, 0), 2);
        }
        // --- 关键步骤：释放C++库分配的内存 ---
        std::cout << "处理完成，正在调用 FreeFaceData() 释放内存..." << std::endl;
        FreeFaceData(faceResults);

        // 将结果保存为文件
        std::string output_path = "result.jpg";
        cv::imwrite(output_path, image);
        std::cout << "\n结果已保存到文件: " << output_path << std::endl;
    }else
    {
        std::cout << "图片中未检测到人脸。" << std::endl;
    }    

    std::cout << "------------------------------------" << std::endl;
    // --- 步骤 6: 反初始化引擎，释放资源 ---
    std::cout << "正在调用 UninitFaceEngine()..." << std::endl;
    UninitFaceEngine();
    std::cout << "引擎已成功反初始化。" << std::endl;

    return 0;
}