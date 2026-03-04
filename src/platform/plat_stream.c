/*
* 硬件平台采集编码
* vi->vpss->venc
*
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <osalog.h>
#include <osamacro.h>

//sdk
#include "hi_defines.h"
#include "hi_type.h"
#include "hi_comm_venc.h"
#include <plat_stream.h>
#include <plat_stream_priv.h>


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

HANDLE os_stream_create(int chn, os_strparam *param)
{
	HI_S32 s32Ret;
	HI_S32 VpssGrp = 0;
	HI_S32 s32StreamNum = 1;
	os_stream_ctx *ctx = NULL;
	HI_BOOL  abChnEnable[VPSS_MAX_PHY_CHN_NUM] = { 1, 0, 0 };
	SIZE_S stSize[2] = {{1920, 1080}, {0, 0}};
    VI_PIPE ViPipe = 0;
    VI_CHN ViChn = 0;
	OS_RC_E enRcMode = OS_RC_CBR;
	VENC_GOP_MODE_E enGopMode = VENC_GOPMODE_NORMALP;
	VPSS_CHN VpssChn[2] 	= { 0, -1 };
	VENC_CHN VencChn[2]	    = { 0, -1 };
	PAYLOAD_TYPE_E	enPayLoad[2]  = {PT_H264, PT_BUTT};
	HI_U32          u32Profile[2] = { 0, 0 };
	OS_SIZE_E enSize[2] = {PIC_1080P, PIC_BUTT};
	HI_BOOL bRcnRefShareBuf = HI_TRUE;
	
	ctx = malloc(sizeof(os_stream_ctx));
	memset(ctx, 0, sizeof(os_stream_ctx));

	os_vi_cfg *vicfg = &ctx->stViCfg;
	//vi base set
	vicfg->width  = param->width;
	vicfg->height = param->height;
	vicfg->fps    = param->fps;
	vicfg->bDelay = HI_FALSE;
	vicfg->enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;
	vicfg->enWDRMode      = WDR_MODE_NONE;

	//vi chn set
    vicfg->iWorkingViNum = 1;
	vicfg->aiWorkingViId[0] = 0;
    vicfg->astViInfo[0].stDevInfo.ViDev = 0;
    vicfg->astViInfo[0].stPipeInfo.aPipe[0] = ViPipe;
    vicfg->astViInfo[0].stChnInfo.ViChn = ViChn;
    vicfg->astViInfo[0].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    vicfg->astViInfo[0].stChnInfo.enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
	vicfg->astViInfo[0].stPipeInfo.aPipe[1] = -1;
	vicfg->astViInfo[0].stPipeInfo.aPipe[2] = -2;
	vicfg->astViInfo[0].stPipeInfo.aPipe[3] = -3;
	vicfg->astViInfo[0].stChnInfo.enVideoFormat  = VIDEO_FORMAT_LINEAR;
	vicfg->astViInfo[0].stChnInfo.enCompressMode = COMPRESS_MODE_SEG;
	vicfg->astViInfo[0].stSnsInfo.lane_divide_mode = LANE_DIVIDE_MODE_0;
	s32Ret = OS_VI_Init(ctx);
	if (s32Ret != AV_R_SOK)
	{
		OSALOG_ERROR("OS_VI_Init failed!\n");
		goto EXIT;
	}
	
	s32Ret = OS_VPSS_Init(VpssGrp, abChnEnable, DYNAMIC_RANGE_SDR8, PIXEL_FORMAT_YVU_SEMIPLANAR_420, stSize,
        vicfg->astViInfo[0].stSnsInfo.enSnsType);
	if (s32Ret != AV_R_SOK)
	{
		OSALOG_ERROR("OS_VPSS_Init failed!\n");
		goto EXIT;
	}

	s32Ret = OS_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
	if (s32Ret != AV_R_SOK)
	{
		OSALOG_ERROR("OS_VPSS_Init failed!\n");
		goto EXIT;
	}
	
    s32Ret = OS_COMM_VENC_Start(VencChn[0], enPayLoad[0], enSize[0], enRcMode, u32Profile[0], bRcnRefShareBuf,
        enGopMode);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("Venc Start failed for %#x!\n", s32Ret);
        goto EXIT;
    }

    s32Ret = OS_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[0], VencChn[0]);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("Venc Get GopAttr failed for %#x!\n", s32Ret);
        goto EXIT;
    }

    s32Ret = OS_COMM_VENC_StartGetStream(VencChn, s32StreamNum, param->ops.getStreamCb);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("Start Venc failed!\n");
        goto EXIT;
    }

	return (HANDLE)ctx;
	
EXIT:
	free(ctx);
	return NULL;
}

int os_stream_set_param(HANDLE hnd, os_strparam *param)
{

	return AV_R_SOK;
}
int os_stream_get_param(HANDLE hnd, os_strparam *param)
{

	return AV_R_SOK;
}

int os_stream_get_frame(HANDLE hnd)
{

	return AV_R_SOK;
}

int os_stream_release_frame(HANDLE hnd)
{

	return AV_R_SOK;
}

int os_stream_requestIDR(int chn, int bInstant)
{
	return OS_COMM_VENC_RequestIDR(chn, bInstant);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

