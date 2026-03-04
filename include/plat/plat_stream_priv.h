#ifndef __PLAT_STREAM_PRIV_H__
#define __PLAT_STREAM_PRIV_H__

#include "plat_stream.h"
#include "osamacro.h"
#include "hi_type.h"
#include "hi_mipi.h"
#include "hi_comm_vi.h"
#include "hi_comm_isp.h"
#include "hi_comm_vpss.h"
#include "hi_comm_sys.h"
#include "hi_comm_venc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define DEFAULT_COMM_VB_NUM 10
#define WDR_MAX_PIPE_NUM    4
#define FILE_NAME_LEN       128
#define PATH_MAX            256

#define MAX_SENSOR_NUM      2
#define ISP_MAX_DEV_NUM     4
static pthread_t    g_IspPid[ISP_MAX_PIPE_NUM] = {0};
static HI_U32       g_au32IspSnsId[ISP_MAX_DEV_NUM] = {0, 1};

typedef enum OS_SNS_TYPE_E {
    SONY_IMX307_MIPI_2M_30FPS_12BIT,
}OS_SNS_TYPE_E;

typedef struct {
    HI_S32              s32SnsId;
    HI_S32              s32BusId;
    combo_dev_t         MipiDev;
	lane_divide_mode_t  lane_divide_mode;
	OS_SNS_TYPE_E       enSnsType;
} os_sensorinfo_s;

typedef struct  {
    HI_BOOL  bSnap;
    HI_BOOL  bDoublePipe;
    VI_PIPE    VideoPipe;
    VI_PIPE    SnapPipe;
    VI_VPSS_MODE_E  enVideoPipeMode;
    VI_VPSS_MODE_E  enSnapPipeMode;
} os_vi_snapinfo_s;

typedef struct {
    VI_DEV      ViDev;
    WDR_MODE_E  enWDRMode;
} os_vi_devinfo_s;

typedef struct  {
    VI_PIPE         aPipe[WDR_MAX_PIPE_NUM];
    VI_VPSS_MODE_E  enMastPipeMode;
    HI_BOOL         bMultiPipe;
    HI_BOOL         bVcNumCfged;
    HI_BOOL         bIspBypass;
    PIXEL_FORMAT_E  enPixFmt;
    HI_U32          u32VCNum[WDR_MAX_PIPE_NUM];
} os_vi_pipeinfo_s;

typedef struct  {
    VI_CHN              ViChn;
    PIXEL_FORMAT_E      enPixFormat;
    DYNAMIC_RANGE_E     enDynamicRange;
    VIDEO_FORMAT_E      enVideoFormat;
    COMPRESS_MODE_E     enCompressMode;
} os_vi_chninfo_s;

typedef struct {
    os_sensorinfo_s       stSnsInfo;
    os_vi_devinfo_s       stDevInfo;
    os_vi_pipeinfo_s      stPipeInfo;
    os_vi_chninfo_s       stChnInfo;
    os_vi_snapinfo_s      stSnapInfo;
} os_vi_info_s;

typedef struct{
	int width;
	int height;
	int fps;
	WDR_MODE_E enWDRMode;
	VI_DEV     ViDev;
	BOOL       bDelay;
    VI_VPSS_MODE_E  enMastPipeMode;
	int             iWorkingViNum;
	int             aiWorkingViId[VI_MAX_DEV_NUM];
	os_vi_info_s    astViInfo[VI_MAX_DEV_NUM];
}os_vi_cfg;

typedef struct{
	VB_CONFIG_S stVbConfig;
}os_vb_cfg;

typedef struct{
	os_vb_cfg stVbCfg;
	os_vi_cfg stViCfg;
}os_stream_ctx;

typedef struct  {
    HI_BOOL bThreadStart;
    VENC_CHN VeChn[VENC_MAX_CHN_NUM];
	fGetVideoStreamCb fUserGetframecb;
    HI_S32  s32Cnt;
} OS_VENC_GETSTREAM_PARA_S;



//platform func
HI_S32 OS_COMM_SYS_Init(VB_CONFIG_S* pstVbConfig);
void OS_COMM_SYS_Exit(void);
HI_S32 OS_VI_Init(os_stream_ctx *ctx);
HI_S32 OS_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL *pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,
    PIXEL_FORMAT_E enPixelFormat, SIZE_S stSize[], OS_SNS_TYPE_E enSnsType);
HI_S32 OS_VI_Bind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp);
HI_S32 OS_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, OS_SIZE_E enSize, OS_RC_E enRcMode,
    HI_U32 u32Profile, HI_BOOL bRcnRefShareBuf, VENC_GOP_MODE_E enGopMode);

HI_S32 OS_COMM_VPSS_Bind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn);
HI_S32 OS_COMM_VENC_StartGetStream(VENC_CHN VeChn[], HI_S32 s32Cnt, fGetVideoStreamCb cb);
HI_S32 OS_COMM_VENC_RequestIDR(VENC_CHN chn, HI_BOOL bInstant);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif


