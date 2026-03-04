#include <iostream>
#include <chrono>
#include <osalog.h>
#include <osamacro.h>
#include <thread>
#include <memory>
#include "plat_mpp.h"
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>


static std::shared_ptr<std::thread> OmniAlgoWorkThread;
static bool bRunning;


// 定义YUV图片参数（根据实际图片调整）
#define ALG_PIC_WIDTH     640    // 图片宽度
#define ALG_PIC_HEIGHT    360    // 图片高度

// 人脸检测结果结构体
typedef struct {
    int x;          // 人脸左上角x坐标
    int y;          // 人脸左上角y坐标
    int width;      // 人脸宽度
    int height;     // 人脸高度
    float confidence; // 可信度（0-1，级联分类器无原生置信度，这里做归一化）
} FACE_DETECT_RESULT_S;

/**
 * @brief YUV420SP(NV12)转OpenCV BGR格式
 * @param yuv_buf YUV数据缓冲区
 * @param width 图片宽度
 * @param height 图片高度
 * @return BGR格式的Mat对象
 */
cv::Mat Yuv420spToBgr(unsigned char *yuv_buf, int width, int height)
{
    // 1. 先将YUV420SP转为YUV420P(I420)
    cv::Mat yuv_mat(height * 3 / 2, width, CV_8UC1, yuv_buf);
    cv::Mat bgr_mat;
    // 2. OpenCV的cvtColor支持NV12转BGR（CV_IMWRITE_JPEG_QUALITY）
    cv::cvtColor(yuv_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);
    return bgr_mat;
}

/**
 * @brief 执行人脸检测（修正可信度计算）
 * @param bgr_mat BGR格式图片
 * @param cascade 人脸分类器
 * @param result 输出人脸检测结果
 * @param max_num 最大检测结果数
 * @return 检测到的人脸数，负数表示失败
 */
int DoFaceDetect(cv::Mat &bgr_mat, cv::CascadeClassifier &cascade, 
				 FACE_DETECT_RESULT_S *result, int max_num)
{
	if (bgr_mat.empty()) {
		OSALOG_ERROR("Input BGR mat is empty!\n");
		return -1;
	}

	cv::Mat input_mat = bgr_mat;
	if (bgr_mat.cols > ALG_PIC_WIDTH || bgr_mat.rows > ALG_PIC_HEIGHT) {
		float scale = std::min(ALG_PIC_WIDTH * 1.0 / bgr_mat.cols, ALG_PIC_HEIGHT * 1.0 / bgr_mat.rows);
		cv::resize(bgr_mat, input_mat, cv::Size(), scale, scale);
		//OSALOG_INFO("DEBUG: input scaled from %dx%d to %dx%d\n", bgr_mat.cols, bgr_mat.rows, input_mat.cols, input_mat.rows);
	}

	// 1. 转为灰度图+直方图均衡化（提升检测准确率）
	cv::Mat gray_mat;
	cv::cvtColor(input_mat, gray_mat, cv::COLOR_BGR2GRAY);
	cv::equalizeHist(gray_mat, gray_mat);

	// 2. 执行人脸检测（获取检测评分，关键！）
	std::vector<cv::Rect> faces;
	std::vector<int> rejectLevels;	// 拒绝等级（评分越低，检测越可靠）
	std::vector<double> levelWeights; // 等级权重（可作为置信度参考）

	if(gray_mat.empty()){
		OSALOG_ERROR("gray_mat error!\n");
		return -1;
	}

#if 0
	OSALOG_INFO("DEBUG before call: cols=%d, rows=%d, channels=%d, data=%p, isContinuous=%d\n",
	   gray_mat.cols, gray_mat.rows, gray_mat.channels(), 
	   gray_mat.data, gray_mat.isContinuous()); 
#endif
	cascade.detectMultiScale(
		gray_mat,				 // 输入灰度图
		faces,					 // 输出人脸矩形
		rejectLevels,			 // 输出拒绝等级
		levelWeights,			 // 输出等级权重
		1.15,					 // 缩放因子
		5,						 // 邻域数阈值
		cv::CASCADE_SCALE_IMAGE, // 检测标志
		cv::Size(30, 30),		 // 最小人脸尺寸
		cv::Size(), 	         // 最大人脸尺寸
		true					 // 输出评分（必须设为true）
	);

	// 3. 解析检测结果（正确的可信度计算）
	int face_num = std::min((int)faces.size(), max_num);  
	
	float scale_back = (bgr_mat.cols > ALG_PIC_WIDTH) ?	
		(float)bgr_mat.cols / input_mat.cols : 1.0f;  
	
	float total_pixels = (float)(input_mat.cols * input_mat.rows); // 注意：这里改用检测图的像素
	
	int valid_face_count = 0;
	
	for (int i = 0; i < face_num; i++) {
		// [新增] 置信度过滤：忽略评分过低的结果
		float score = levelWeights[i];
		float confidence_threshold = 0.5f; // 根据实际效果调整，越高越严格
		
		if (score < confidence_threshold) {
			OSALOG_INFO("DEBUG: Filtered low score face: score=%.2f\n", score);
			continue; // 跳过这个误报
		}
	
		result[valid_face_count].x = faces[i].x * scale_back;
		result[valid_face_count].y = faces[i].y * scale_back;
		result[valid_face_count].width = faces[i].width * scale_back;
		result[valid_face_count].height = faces[i].height * scale_back;
	
		// 计算可信度
		float level_score = 1.0f / (1.0f + rejectLevels[i]);
		float size_ratio = (float)(faces[i].width * faces[i].height) / total_pixels;
		size_ratio = std::min(std::max(size_ratio, 0.01f), 0.2f) / 0.2f;
		
		// 综合评分：加入原始检测评分
		result[valid_face_count].confidence = 0.5f * level_score + 0.2f * size_ratio + 0.3f * (float)score;
		
		valid_face_count++;
		
		// 防止数组越界
		if (valid_face_count >= max_num) 
			break;
	}

	return valid_face_count;

}




/**
 * @brief 初始化人脸检测（加载分类器）
 * @param cascade 人脸分类器对象
 * @param cascade_path 分类器xml文件路径
 * @return 0成功，非0失败
 */
int FaceDetectInit(cv::CascadeClassifier &cascade, const char *cascade_path)
{
    if (!cascade.load(cascade_path)) {
        OSALOG_ERROR("Load face cascade file %s failed!\n", cascade_path);
        return -1;
    }

	OSALOG_INFO("Load face cascade file %s success!\n", cascade_path);
    return 0;
}


static cv::CascadeClassifier face_cascade;
void OmniAlgoWork_start()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	OSALOG_INFO("Face Detect mpp init....\n");
	if (0 != mpp_algo_init(ALG_PIC_WIDTH, ALG_PIC_HEIGHT))
	{
		OSALOG_ERROR("Face Detect mpp failed!\n");
		return;
	}		
	
	OSALOG_INFO("Face Detect load model....\n");
	FaceDetectInit(face_cascade, "./haarcascade_frontalface_alt2.xml");

	if (face_cascade.empty())
	{
		OSALOG_ERROR("Face Detect load model failed!\n");
		return;
	}
	OSALOG_INFO("Face Detect load model success\n");

	bRunning = true;
	OmniAlgoWorkThread = std::make_shared<std::thread>([&](){
		while(bRunning) {
			vpp_frame_info_t stVppFrameInfo;
			if (AV_R_SOK == mpp_vpp_getframe(0, &stVppFrameInfo))
			{
				//OSALOG_INFO("get frame[%d, %d], send to opencv \n", stVppFrameInfo.frame.width, stVppFrameInfo.frame.height);
				auto now = std::chrono::system_clock::now();
				auto timestamp_start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					now.time_since_epoch()
				).count();

				do {
					// 1. YUV转BGR
					cv::Mat bgr_mat = Yuv420spToBgr(stVppFrameInfo.frame.data[0], stVppFrameInfo.frame.width, stVppFrameInfo.frame.height);
					if (bgr_mat.empty()) {
						printf("YUV to BGR failed!\n");
						break;
					}
					
					// 2. 执行人脸检测
					FACE_DETECT_RESULT_S face_result[2];  // 最多存储2个人脸
					int face_num = DoFaceDetect(bgr_mat, face_cascade, face_result, 2);
					if (face_num < 0) {
						break;
					}
					
					// 3. 输出检测结果
					if (face_num == 0)
						break;
				
					OSALOG_INFO("Detect face num: %d\n", face_num);
					for (int i = 0; i < face_num; i++) {
						OSALOG_INFO("Face %d: x=%d, y=%d, width=%d, height=%d, confidence=%.2f\n",
							   i+1,
							   face_result[i].x,
							   face_result[i].y,
							   face_result[i].width,
							   face_result[i].height,
							   face_result[i].confidence);
					}


					{
						// 保存大图
						static int cnt;
						std::string path = "./face_bigpic_" + std::to_string(cnt++) + ".jpg";
						cv::imwrite(path, bgr_mat);
					}
				}while(0);

				stVppFrameInfo.free(stVppFrameInfo.priv);
				
				now = std::chrono::system_clock::now();
				auto timestamp_end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					now.time_since_epoch()
				).count();
				OSALOG_INFO("algo process frame s:%lld e:%lld cost:%lld\n", static_cast<long long>(timestamp_start_ms), static_cast<long long>(timestamp_end_ms), 
					static_cast<long long>(timestamp_end_ms - timestamp_start_ms));
			}

			
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}	
	);
}

void OmniAlgoWork_stop()
{
	bRunning = false;
	if (OmniAlgoWorkThread->joinable())
		OmniAlgoWorkThread->join();


}

