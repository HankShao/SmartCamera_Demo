/*
* 主程序
*/

#include <iostream>
#include <thread>
#include <memory>
#include <osalog.h>
#include <osamacro.h>
#include <plat_stream.h>

using namespace std;
extern void Omni_GetVideoStreamCb(uint8_t*, int32_t);
extern void Omnistream_work_start(void);
extern void Omnistream_work_stop(void);
extern void OmniAlgoWork_start();
extern void OmniAlgoWork_stop();


int main(int argc, char *argv[])
{
	cout << "hi3516cv500 ipcamera starting ..." << endl;

	// 启动 OmniStream 网络服务
	Omnistream_work_start();

	// 启动海思取流线程，通过回调获取帧
	os_strparam param;
	param.width = 1920;
	param.height = 1080;
	param.fps = 30;
	param.ops.getStreamCb = Omni_GetVideoStreamCb;
	os_stream_create(0, &param);

	OmniAlgoWork_start();

	printf("please press twice ENTER to exit this sample\n");
	getchar();
	getchar();
	
	Omnistream_work_stop();
	OmniAlgoWork_stop();

	return AV_R_SOK;
}

