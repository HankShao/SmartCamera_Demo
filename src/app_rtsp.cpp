#include <iostream>
#include <osalog.h>
#include <osamacro.h>
#include <thread>
#include <memory>
#include "OmniStream.h"
#include "plat_stream.h"

using namespace std;
using namespace OmniStream;

// 1.应用层实现回调接口
class MyAppHandler:public OmniStream::IServerCallbacks {
public:
    void OnRequestKeyFrame(const std::string& streamPath) override {
        std::cout << "[App] Rec OmniStream req Path: " << streamPath << " Need keyFrmame!" << std::endl;
        os_stream_requestIDR(0, 1);
        // Encoder.ForceIDR()
        return;
    }

    void OnClientEvent(const std::string& clientIp, bool connected) override {
        std::cout << "[App] Client " << clientIp << (connected?" connected":" disconnected") << std::endl;

        return;
    }
};

static std::unique_ptr<IOmniServer> OmniServer;
static std::unique_ptr<IOmniServer> OmniServerOnvif;
static MyAppHandler MyAppHandler;
static std::shared_ptr<std::thread> OmniWorkThread;
static std::shared_ptr<std::thread> OmniWorkThreadOnvif;

void Omnistream_work_start(void)
{
    // 2.创建RTSP服务器
    OmniServer = OmniStream::OmniFactory::CreateServer("RTSP");

    // 3.实例化回调处理对象
    OmniServer->RegisterCallbacks(&MyAppHandler);

    // 4.配置并启动
    OmniStream::ServerConfig cfg;
    cfg.rtspPort = 8554;
    OmniServer->Init(cfg);

    OmniWorkThread = std::make_shared<std::thread>([&](){ 
			if (!OmniServer) {
				cout << "[ERROR][Omnistream_work_start] Omniserver is nullptr!" << endl;
				return;
			}
			cout << "[INFO][Omnistream_work_start] Omniserver is nullptr!" << endl;
			OmniServer->Start();
		}
	);


#if 0
    // 5.模拟数据产生
    while (true) {
        auto frame = std::make_shared<OmniStream::MediaFrame>();

        //填充数据
        OmniServer->SendFrame(frame);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

	srv_thread.join();
#endif	

}

void Omnistream_onvif_work()
{
	OmniServerOnvif = OmniStream::OmniFactory::CreateServer("ONVIF");
    OmniStream::ServerConfig cfg;
    OmniServerOnvif->Init(cfg);

    OmniWorkThreadOnvif = std::make_shared<std::thread>([&](){ 
			if (!OmniServerOnvif) {
				cout << "[ERROR][Omnistream_work_start] Omniserver is nullptr!" << endl;
				return;
			}
			cout << "[INFO][Omnistream_work_start] Omniserver is nullptr!" << endl;
			OmniServerOnvif->Start();
		}
	);

}


void Omni_GetVideoStreamCb(uint8_t* data, int32_t len)
{
//	std::shared_ptr<void> frame(data, [](void *p){ std::free(p);} );
//  cout << "Omnistream feed frame addr:" << static_cast<void *>(data) << " len:" << len << endl;

	auto frame = std::make_shared<OmniStream::MediaFrame>();
	static uint32_t utimestamp;
	
	// 将 malloc 申请的内存数据复制到 vector<uint8_t>
	utimestamp += 3600; // 按照90kHz计算时间戳
	frame->data.insert(frame->data.end(), data, data + len);
	frame->timestamp = utimestamp;

	//填充数据
    OmniServer->SendFrame(frame);

	std::free(data);
}

void Omnistream_work_stop(void)
{
	cout << "Omnistream server stopping..." << endl;
	OmniServer->Stop();
	
	if (OmniWorkThread->joinable())
		OmniWorkThread->join();

	OmniServerOnvif->Stop();
	if (OmniWorkThreadOnvif->joinable())
		OmniWorkThreadOnvif->join();

	cout << "Omnistream server stopping success." << endl;
}




