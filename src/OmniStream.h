#ifndef OMNI_STREAM_API_H
#define OMNI_STREAM_API_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

#define OMNI_API __attribute__((visibility("default")))

namespace OmniStream {

/**
 * @brief 媒体帧类型定义，直接在公有头文件定义，避免包含内部文件
 */
enum class FrameType {
    VIDEO_H264 = 96,
    VIDEO_H265 = 98,
    AUDIO_AAC  = 97
};

/**
 * @brief 媒体帧数据结构
 */
struct MediaFrame {
    FrameType eType;
    uint32_t timestamp;    //单调递增（采样率90kHz，25fps=3600/帧）
    bool isKeyFrame;
    std::vector<uint8_t> data;
    using Ptr = std::shared_ptr<MediaFrame>;
};

/**
 * @brief 应用层需要实现的回调接口族
 * 用于处理库向应用层发出的指令或请求
 */
class IServerCallbacks {
public:
    virtual ~IServerCallbacks()=default;

    // 当有新客户端播放时，库通知应用层：请强制产生一个I帧已供快速秒开
    virtual void OnRequestKeyFrame(const std::string& streamPath) = 0;

    // 预留：码率自适应调整请求
    virtual void OnBitrateChange(int targetBitrateKbps) {}

    // 预留：连接事件通知
    virtual void OnClientEvent(const std::string& clientIp, bool connected){}
};

/**
 * @brief 服务配置
 */
struct ServerConfig {
    std::string ip = "0.0.0.0";
    int OnvifPort = 3702;
    int HttpPort = 8080;
    int rtspPort = 8554;
    int workThreads = 4;

};

/**
 * @brief 抽象服务端接口
 */
class IOmniServer {
public:
    virtual ~IOmniServer() = default;

    // 初始化服务
    virtual bool Init(const ServerConfig& cfg) = 0;
    
    // 注册回调函数
    virtual void RegisterCallbacks(IServerCallbacks* cb) = 0;

    virtual bool Start() = 0;

    virtual void Stop() = 0;

    virtual bool IsRunning() = 0;

    // 依然保留发送接口，但应用层通常在收到回调后调用此接口
    virtual void SendFrame(MediaFrame::Ptr frame) = 0;
};

/**
 * OmniStream工厂类，用于创建实例服务器
 */
class OMNI_API OmniFactory{
public:
    static std::unique_ptr<IOmniServer> CreateServer(const std::string& protocolType);
};

}// namespace OmniStream

#endif