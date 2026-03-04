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
#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_ae_comm.h"
#include "hi_awb_comm.h"
#include "hi_comm_isp.h"
#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_isp.h"
#include "mpi_vi.h"
#include "mpi_ae.h"
#include "mpi_awb.h"
#include "hi_buffer.h"
#include "plat_stream_priv.h"
#include "plat_isp_cfg.h"

#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif

int OS_COMM_VI_SetParam(os_vi_cfg *pstViConfig)
{
    HI_S32              i, j;
    HI_S32              s32ViNum;
    HI_S32              s32Ret = HI_FAILURE;
    VI_PIPE             ViPipe;
    VI_VPSS_MODE_S      stVIVPSSMode;
    os_vi_info_s        *pstViInfo = HI_NULL;

    if (pstViConfig == HI_NULL) {
        OSALOG_ERROR("%s: null ptr\n", __FUNCTION__);
        return s32Ret;
    }

    s32Ret = HI_MPI_SYS_GetVIVPSSMode(&stVIVPSSMode);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("Get VI-VPSS mode Param failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
        s32ViNum  = pstViConfig->aiWorkingViId[i];
        pstViInfo = &pstViConfig->astViInfo[s32ViNum];
        ViPipe    = pstViInfo->stPipeInfo.aPipe[0];
        stVIVPSSMode.aenMode[ViPipe] = pstViInfo->stPipeInfo.enMastPipeMode;
        if ((pstViInfo->stPipeInfo.bMultiPipe == HI_TRUE) ||
            (pstViInfo->stPipeInfo.enMastPipeMode == VI_OFFLINE_VPSS_ONLINE)) {
		    if (stVIVPSSMode.aenMode[0] == VI_OFFLINE_VPSS_ONLINE) {
		        for (i = 0; i < VI_MAX_PIPE_NUM; i++) {
		            stVIVPSSMode.aenMode[i] = stVIVPSSMode.aenMode[0];
		        }
		    }
            for (j = 1; j < WDR_MAX_PIPE_NUM; j++) {
                ViPipe = pstViInfo->stPipeInfo.aPipe[j];
                if (ViPipe != -1) {
                    stVIVPSSMode.aenMode[ViPipe] = pstViInfo->stPipeInfo.enMastPipeMode;
                }
            }
        }

        if ((pstViInfo->stSnapInfo.bSnap) && (pstViInfo->stSnapInfo.bDoublePipe)) {
            ViPipe = pstViInfo->stPipeInfo.aPipe[1];
            if (ViPipe != -1) {
                stVIVPSSMode.aenMode[ViPipe] = pstViInfo->stSnapInfo.enSnapPipeMode;
            }
        }
    }

    s32Ret = HI_MPI_SYS_SetVIVPSSMode(&stVIVPSSMode);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("Set VI-VPSS mode Param failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return AV_R_SOK;
}

#define MIPI_DEV_NODE       "/dev/hi_mipi"
/*****************************************************************************
* function : init mipi
*****************************************************************************/
HI_S32 OS_COMM_VI_StartMIPI(os_vi_cfg *pstViConfig)
{
    HI_S32 s32Ret = HI_FAILURE;
	HI_S32 i;
    HI_S32 fd = -1;
	HI_S32 s32ViNum;
	combo_dev_t devno;
	os_vi_info_s *pstViInfo;
	sns_clk_source_t SnsDev = 0;
	
    if (pstViConfig == HI_NULL) {
        OSALOG_ERROR("%s: null ptr\n", __FUNCTION__);
        return s32Ret;
    }

    fd = open(MIPI_DEV_NODE, O_RDWR);
    if (fd < 0) {
        OSALOG_ERROR("open hi_mipi dev failed\n");
        return s32Ret;
    }
	/* 1.配置mipi驱动模式 */
    s32Ret = ioctl(fd, HI_MIPI_SET_HS_MODE, &pstViConfig->astViInfo[0].stSnsInfo.lane_divide_mode);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("HI_MIPI_SET_HS_MODE failed\n");
    }

	/* 2.使能mipi时钟 */
    for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
        s32ViNum  = pstViConfig->aiWorkingViId[i];
        pstViInfo = &pstViConfig->astViInfo[s32ViNum];
        devno = pstViInfo->stSnsInfo.MipiDev;
        s32Ret = ioctl(fd, HI_MIPI_ENABLE_MIPI_CLOCK, &devno);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("MIPI_ENABLE_CLOCK %d failed\n", devno);
            goto EXIT;
        }
    }

	/* 3.复位mipi */
    for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
        s32ViNum  = pstViConfig->aiWorkingViId[i];
        pstViInfo = &pstViConfig->astViInfo[s32ViNum];
        devno = pstViInfo->stSnsInfo.MipiDev;
        s32Ret = ioctl(fd, HI_MIPI_RESET_MIPI, &devno);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("RESET_MIPI %d failed\n", devno);
            goto EXIT;
        }
    }

	/* 4.使能sensor时钟 */
    for (SnsDev = 0; SnsDev < SNS_MAX_CLK_SOURCE_NUM; SnsDev++) {
        s32Ret = ioctl(fd, HI_MIPI_ENABLE_SENSOR_CLOCK, &SnsDev);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("HI_MIPI_ENABLE_SENSOR_CLOCK failed\n");
            goto EXIT;
        }
    }

	/* 5.复位sensor */
    for (SnsDev = 0; SnsDev < SNS_MAX_RST_SOURCE_NUM; SnsDev++) {
        s32Ret = ioctl(fd, HI_MIPI_RESET_SENSOR, &SnsDev);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("HI_MIPI_RESET_SENSOR failed\n");
            goto EXIT;
        }
    }

	usleep(10000);  /* sleep 10000us to avoid reset signal invalid. */

	/* 6.根据硬件接线配置mipi属性 */
	combo_dev_attr_t  stcomboDevAttr;
    for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
        s32ViNum  = pstViConfig->aiWorkingViId[i];
        pstViInfo = &pstViConfig->astViInfo[s32ViNum];
		(hi_void)memcpy_s(&stcomboDevAttr, sizeof(combo_dev_attr_t), \
		&MIPI_4lane_CHN0_SENSOR_IMX307_12BIT_2M_NOWDR_ATTR, sizeof(combo_dev_attr_t));
        stcomboDevAttr.devno = pstViInfo->stSnsInfo.MipiDev;

        OSALOG_INFO("MipiDev%d SetMipiAttr enWDRMode: %d\n",
            pstViInfo->stSnsInfo.MipiDev, pstViInfo->stDevInfo.enWDRMode);

        s32Ret = ioctl(fd, HI_MIPI_SET_DEV_ATTR, &stcomboDevAttr);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("MIPI_SET_DEV_ATTR failed.\n");
            goto EXIT;
        }
    }	

	/* 6.解除mipi复位状态 */
    for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
        s32ViNum  = pstViConfig->aiWorkingViId[i];
        pstViInfo = &pstViConfig->astViInfo[s32ViNum];
        devno = pstViInfo->stSnsInfo.MipiDev;
        s32Ret = ioctl(fd, HI_MIPI_UNRESET_MIPI, &devno);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("UNRESET_MIPI %d failed\n", devno);
            goto EXIT;
        }
    }

	/* 7.解除sensor复位状态 */
    for (SnsDev = 0; SnsDev < SNS_MAX_RST_SOURCE_NUM; SnsDev++) {
        s32Ret = ioctl(fd, HI_MIPI_UNRESET_SENSOR, &SnsDev);
        if (s32Ret != HI_SUCCESS) {
            OSALOG_ERROR("HI_MIPI_UNRESET_SENSOR failed\n");
            goto EXIT;
        }
    }

EXIT:
    close(fd);
    return s32Ret;
}

static HI_S32 OS_COMM_VI_StartViPipe(os_vi_info_s *pstViInfo)
{
    HI_S32          i, j;
    HI_S32          s32Ret;
    VI_PIPE         ViPipe;
    VI_PIPE_ATTR_S  stPipeAttr;

    for (i = 0; i < WDR_MAX_PIPE_NUM; i++) {
        if ((pstViInfo->stPipeInfo.aPipe[i] >= 0) && (pstViInfo->stPipeInfo.aPipe[i] < VI_MAX_PIPE_NUM)) {
            ViPipe = pstViInfo->stPipeInfo.aPipe[i];
            (hi_void)memcpy_s(&stPipeAttr, sizeof(VI_PIPE_ATTR_S),
                &PIPE_ATTR_1920x1080_RAW12_420_3DNR_RFR, sizeof(VI_PIPE_ATTR_S));		
            if (pstViInfo->stPipeInfo.bIspBypass == HI_TRUE) {
                stPipeAttr.bIspBypass = HI_TRUE;
                stPipeAttr.enPixFmt   = pstViInfo->stPipeInfo.enPixFmt;
                stPipeAttr.enBitWidth = DATA_BITWIDTH_8;
            }

            if ((ViPipe == 2) || (ViPipe == 3)) {  /* V500 pipe2 and pipe3 only support cmp_none */
                stPipeAttr.enCompressMode = COMPRESS_MODE_NONE;
            }
			
            if ((pstViInfo->stSnapInfo.bSnap) &&
                (pstViInfo->stSnapInfo.bDoublePipe) &&
                (ViPipe == pstViInfo->stSnapInfo.SnapPipe)) {
                s32Ret = HI_MPI_VI_CreatePipe(ViPipe, &stPipeAttr);
                if (s32Ret != HI_SUCCESS) {
                    OSALOG_ERROR("HI_MPI_VI_CreatePipe failed with %#x!\n", s32Ret);
                    goto EXIT;
                }
            } else {
                s32Ret = HI_MPI_VI_CreatePipe(ViPipe, &stPipeAttr);
                if (s32Ret != HI_SUCCESS) {
                    OSALOG_ERROR("HI_MPI_VI_CreatePipe failed with %#x!\n", s32Ret);
                    return s32Ret;
                }

                if (pstViInfo->stPipeInfo.bVcNumCfged == HI_TRUE) {
                    s32Ret = HI_MPI_VI_SetPipeVCNumber(ViPipe, pstViInfo->stPipeInfo.u32VCNum[i]);
                    if (s32Ret != HI_SUCCESS) {
                        HI_MPI_VI_DestroyPipe(ViPipe);
                        OSALOG_ERROR("HI_MPI_VI_SetPipeVCNumber failed with %#x!\n", s32Ret);
                        return s32Ret;
                    }
                }

                s32Ret = HI_MPI_VI_StartPipe(ViPipe);
                if (s32Ret != HI_SUCCESS) {
                    HI_MPI_VI_DestroyPipe(ViPipe);
                    OSALOG_ERROR("HI_MPI_VI_StartPipe failed with %#x!\n", s32Ret);
                    return s32Ret;
                }
            }
        }
    }

    return s32Ret;

EXIT:
    for (j = 0; j < i; j++) {
        ViPipe = j;
		s32Ret = HI_MPI_VI_StopPipe(ViPipe);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VI_StopPipe failed with %#x!\n", s32Ret);
			continue;
		}
		
		s32Ret = HI_MPI_VI_DestroyPipe(ViPipe);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VI_DestroyPipe failed with %#x!\n", s32Ret);
			continue;
		}
    }
    return s32Ret;
}

static HI_S32 OS_COMM_VI_StartViChn(os_vi_info_s *pstViInfo)
{
    HI_S32              i;
    HI_BOOL             bNeedChn;
    HI_S32              s32Ret = HI_SUCCESS;
    VI_PIPE             ViPipe;
    VI_CHN              ViChn;
    VI_CHN_ATTR_S       stChnAttr;
    VI_VPSS_MODE_E      enMastPipeMode;

    for (i = 0; i < WDR_MAX_PIPE_NUM; i++) {
        if ((pstViInfo->stPipeInfo.aPipe[i] >= 0) && (pstViInfo->stPipeInfo.aPipe[i] < VI_MAX_PIPE_NUM)) {
            if (pstViInfo->stDevInfo.enWDRMode == WDR_MODE_NONE) {
                bNeedChn = HI_TRUE;
            } else {
                bNeedChn = (i > 0) ? HI_FALSE : HI_TRUE;
            }
            if (bNeedChn) {
                ViPipe = pstViInfo->stPipeInfo.aPipe[i];
                ViChn  = pstViInfo->stChnInfo.ViChn;
            	(hi_void)memcpy_s(&stChnAttr, sizeof(VI_CHN_ATTR_S),
                	&CHN_ATTR_1920x1080_420_SDR8_LINEAR, sizeof(VI_CHN_ATTR_S));
                stChnAttr.enDynamicRange = pstViInfo->stChnInfo.enDynamicRange;
                stChnAttr.enVideoFormat  = pstViInfo->stChnInfo.enVideoFormat;
                stChnAttr.enPixelFormat  = pstViInfo->stChnInfo.enPixFormat;
                stChnAttr.enCompressMode = pstViInfo->stChnInfo.enCompressMode;
                s32Ret = HI_MPI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);
                if (s32Ret != HI_SUCCESS) {
                    OSALOG_ERROR("HI_MPI_VI_SetChnAttr failed with %#x!\n", s32Ret);
                    return s32Ret;
                }

                enMastPipeMode = pstViInfo->stPipeInfo.enMastPipeMode;
                if ((enMastPipeMode == VI_OFFLINE_VPSS_OFFLINE) ||
                    (enMastPipeMode == VI_ONLINE_VPSS_OFFLINE)) {
                    s32Ret = HI_MPI_VI_EnableChn(ViPipe, ViChn);
                    if (s32Ret != HI_SUCCESS) {
                        OSALOG_ERROR("HI_MPI_VI_EnableChn failed with %#x!\n", s32Ret);
                        return s32Ret;
                    }
                }
            }
        }
    }

    return s32Ret;
}

static HI_S32 OS_COMM_VI_DestroySingleVi(os_vi_info_s *pstViInfo)
{
	return AV_R_SOK;
}

/*****************************************************************************
* function : init vi param
*****************************************************************************/
HI_S32 OS_COMM_VI_CreateVi(os_vi_cfg *pstViConfig)
{
	HI_S32				i, j;
	HI_S32				s32ViNum;
	HI_S32				s32Ret = HI_FAILURE;
	os_vi_info_s    	*pstViInfo = HI_NULL;
	VI_DEV_ATTR_S		stViDevAttr;
	VI_DEV_BIND_PIPE_S	stDevBindPipe = {0};
	HI_S32				s32PipeCnt = 0;

	if (pstViConfig == HI_NULL) {
		OSALOG_ERROR("%s: null ptr\n", __FUNCTION__);
		return s32Ret;
	}

	for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
		s32ViNum  = pstViConfig->aiWorkingViId[i];
		pstViInfo = &pstViConfig->astViInfo[s32ViNum];
		(hi_void)memcpy_s(&stViDevAttr, sizeof(VI_DEV_ATTR_S), &DEV_ATTR_IMX307_2M_BASE, sizeof(VI_DEV_ATTR_S));
		stViDevAttr.stWDRAttr.enWDRMode = pstViInfo->stDevInfo.enWDRMode;
		s32Ret = HI_MPI_VI_SetDevAttr(pstViInfo->stDevInfo.ViDev, &stViDevAttr);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VI_SetDevAttr failed with %#x!\n", s32Ret);
			goto EXIT;
		}
		s32Ret = HI_MPI_VI_EnableDev(pstViInfo->stDevInfo.ViDev);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VI_EnableDev failed with %#x!\n", s32Ret);
			goto EXIT;
		}

		for (i = 0; i < WDR_MAX_PIPE_NUM; i++) {
			if ((pstViInfo->stPipeInfo.aPipe[i] >= 0)  && (pstViInfo->stPipeInfo.aPipe[i] < VI_MAX_PIPE_NUM)) {
				stDevBindPipe.PipeId[s32PipeCnt] = pstViInfo->stPipeInfo.aPipe[i];
				s32PipeCnt++;
				stDevBindPipe.u32Num = s32PipeCnt;
			}
		}
		s32Ret = HI_MPI_VI_SetDevBindPipe(pstViInfo->stDevInfo.ViDev, &stDevBindPipe);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VI_SetDevBindPipe failed with %#x!\n", s32Ret);
			goto EXIT;
		}

		s32Ret = OS_COMM_VI_StartViPipe(pstViInfo);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("OS_COMM_VI_StartViPipe failed !\n");
			goto EXIT;
		}

		s32Ret = OS_COMM_VI_StartViChn(pstViInfo);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("OS_COMM_VI_StartViChn failed !\n");
			goto EXIT;
		}		
	}


	return s32Ret;

EXIT:
	for (j = 0; j < i; j++) {
		s32ViNum  = pstViConfig->aiWorkingViId[j];
		pstViInfo = &pstViConfig->astViInfo[s32ViNum];
		OS_COMM_VI_DestroySingleVi(pstViInfo);
	}
	return s32Ret;
}

HI_S32 OS_COMM_ISP_Sensor_Regiter_callback(ISP_DEV IspDev, HI_U32 u32SnsId)
{
    ALG_LIB_S stAeLib;
    ALG_LIB_S stAwbLib;
    const ISP_SNS_OBJ_S* pstSnsObj = HI_NULL;
    HI_S32    s32Ret = -1;

    if (MAX_SENSOR_NUM <= u32SnsId)
    {
        OSALOG_ERROR("invalid sensor id: %d\n", u32SnsId);
        return HI_FAILURE;
    }

    pstSnsObj = &stSnsImx307Obj;
    if (HI_NULL == pstSnsObj)
    {
        OSALOG_ERROR("sensor %d not exist!\n", u32SnsId);
        return HI_FAILURE;
    }

    stAeLib.s32Id = IspDev;
    stAwbLib.s32Id = IspDev;
    (HI_VOID)strncpy_s(stAeLib.acLibName, sizeof(HI_AE_LIB_NAME) + 1, HI_AE_LIB_NAME, sizeof(HI_AE_LIB_NAME));
    (HI_VOID)strncpy_s(stAwbLib.acLibName, sizeof(HI_AWB_LIB_NAME) + 1, HI_AWB_LIB_NAME, sizeof(HI_AWB_LIB_NAME));

    if (pstSnsObj->pfnRegisterCallback != HI_NULL)
    {
        s32Ret = pstSnsObj->pfnRegisterCallback(IspDev, &stAeLib, &stAwbLib);

        if (s32Ret != HI_SUCCESS)
        {
            OSALOG_ERROR("sensor_register_callback failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        OSALOG_ERROR("sensor_register_callback failed with HI_NULL!\n");
    }

    g_au32IspSnsId[IspDev] = u32SnsId;

    return HI_SUCCESS;
}

HI_S32 OS_COMM_ISP_BindSns(ISP_DEV IspDev, HI_U32 u32SnsId, OS_SNS_TYPE_E enSnsType, HI_S8 s8SnsDev)
{
    ISP_SNS_COMMBUS_U uSnsBusInfo;
    ISP_SNS_TYPE_E enBusType;
    const ISP_SNS_OBJ_S* pstSnsObj = HI_NULL;
    HI_S32 s32Ret;

    if (MAX_SENSOR_NUM <= u32SnsId)
    {
        OSALOG_ERROR("invalid sensor id: %d\n", u32SnsId);
        return HI_FAILURE;
    }

    pstSnsObj = &stSnsImx307Obj;

    if (HI_NULL == pstSnsObj)
    {
        OSALOG_ERROR("sensor %d not exist!\n", u32SnsId);
        return HI_FAILURE;
    }

    enBusType = ISP_SNS_I2C_TYPE;

    if (ISP_SNS_I2C_TYPE == enBusType)
    {
        uSnsBusInfo.s8I2cDev = s8SnsDev;
    }
    else
    {
        uSnsBusInfo.s8SspDev.bit4SspDev = s8SnsDev;
        uSnsBusInfo.s8SspDev.bit4SspCs  = 0;
    }

    if (HI_NULL != pstSnsObj->pfnSetBusInfo)
    {
        s32Ret = pstSnsObj->pfnSetBusInfo(IspDev, uSnsBusInfo);

        if (s32Ret != HI_SUCCESS)
        {
            OSALOG_ERROR("set sensor bus info failed with %#x!\n", s32Ret);
            return s32Ret;
        }
    }
    else
    {
        OSALOG_ERROR("not support set sensor bus info!\n");
        return HI_FAILURE;
    }

    return s32Ret;
}

#define ISP_THREAD_NAME_LEN 20
static void* SAMPLE_COMM_ISP_Thread(void* param)
{
    HI_S32 s32Ret;
    ISP_DEV IspDev;
    HI_CHAR szThreadName[ISP_THREAD_NAME_LEN];

    IspDev = (ISP_DEV)(HI_UINTPTR_T)param;

    s32Ret = snprintf_s(szThreadName, ISP_THREAD_NAME_LEN, ISP_THREAD_NAME_LEN - 1, "ISP%d_RUN", IspDev);
    if (s32Ret < 0 ) {
        OSALOG_ERROR("snprintf_s err !\n");
        exit(-1);
    }

    prctl(PR_SET_NAME, szThreadName, 0,0,0);

    OSALOG_INFO("ISP Dev %d running !\n", IspDev);
    s32Ret = HI_MPI_ISP_Run(IspDev);
    if (HI_SUCCESS != s32Ret)
    {
        OSALOG_ERROR("HI_MPI_ISP_Run failed with %#x!\n", s32Ret);
    }

    return NULL;
}

/******************************************************************************
* funciton : ISP Run
******************************************************************************/
HI_S32 OS_COMM_ISP_Run(ISP_DEV IspDev)
{
    HI_S32 s32Ret = 0;
    pthread_attr_t* pstAttr = NULL;

    s32Ret = pthread_create(&g_IspPid[IspDev], pstAttr, SAMPLE_COMM_ISP_Thread, (HI_VOID*)(HI_UINTPTR_T)IspDev);
    if (0 != s32Ret)
    {
        OSALOG_ERROR("create isp running thread failed!, error: %d, %s\r\n", s32Ret, strerror(s32Ret));
        goto out;
    }

out:
    if (NULL != pstAttr)
    {
        pthread_attr_destroy(pstAttr);
    }

    return s32Ret;
}

HI_S32 OS_COMM_VI_StartIsp(os_vi_info_s *pstViInfo)
{
    HI_S32              i;
    HI_BOOL             bNeedPipe;
    HI_S32              s32Ret = HI_SUCCESS;
    VI_PIPE             ViPipe;
    HI_U32              u32SnsId;
    ISP_PUB_ATTR_S      stPubAttr;
    VI_PIPE_ATTR_S      stPipeAttr;

	(hi_void)memcpy_s(&stPipeAttr, sizeof(VI_PIPE_ATTR_S),
		&PIPE_ATTR_1920x1080_RAW12_420_3DNR_RFR, sizeof(VI_PIPE_ATTR_S));
    if (stPipeAttr.enPipeBypassMode == VI_PIPE_BYPASS_BE) {
        return s32Ret;
    }

    for (i = 0; i < WDR_MAX_PIPE_NUM; i++) {
        if ((pstViInfo->stPipeInfo.aPipe[i] >= 0)  && (pstViInfo->stPipeInfo.aPipe[i] < VI_MAX_PIPE_NUM)) {
            (HI_VOID)memcpy_s(&stPubAttr, sizeof(ISP_PUB_ATTR_S),
                              &ISP_PUB_ATTR_IMX307_MIPI_2M_30FPS, sizeof(ISP_PUB_ATTR_S));
            stPubAttr.enWDRMode = pstViInfo->stDevInfo.enWDRMode;

            ViPipe = pstViInfo->stPipeInfo.aPipe[i];
            if (pstViInfo->stDevInfo.enWDRMode == WDR_MODE_NONE) {
                bNeedPipe = HI_TRUE;
            } else {
                bNeedPipe = (i > 0) ? HI_FALSE : HI_TRUE;
            }
            if (bNeedPipe != HI_TRUE) {
                continue;
            }

            u32SnsId = pstViInfo->stSnsInfo.s32SnsId;
            s32Ret = OS_COMM_ISP_Sensor_Regiter_callback(ViPipe, u32SnsId);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("register sensor %d to ISP %d failed\n", u32SnsId, ViPipe);
                return s32Ret;
            }

            if (((pstViInfo->stSnapInfo.bDoublePipe) && (pstViInfo->stSnapInfo.SnapPipe == ViPipe)) ||
                ((pstViInfo->stPipeInfo.bMultiPipe) && (i > 0))) {
                s32Ret = OS_COMM_ISP_BindSns(ViPipe, u32SnsId, pstViInfo->stSnsInfo.enSnsType, -1);
                if (s32Ret != HI_SUCCESS) {
                    OSALOG_ERROR("register sensor %d bus id %d failed\n", u32SnsId, pstViInfo->stSnsInfo.s32BusId);
                    return s32Ret;
                }
            } else {
                s32Ret = OS_COMM_ISP_BindSns(ViPipe, u32SnsId, pstViInfo->stSnsInfo.enSnsType,
                    pstViInfo->stSnsInfo.s32BusId);
                if (s32Ret != HI_SUCCESS) {
                    OSALOG_ERROR("register sensor %d bus id %d failed\n", u32SnsId, pstViInfo->stSnsInfo.s32BusId);
                    return s32Ret;
                }
            }

			ALG_LIB_S stAeLib;
			stAeLib.s32Id = ViPipe;
			(HI_VOID)strncpy_s(stAeLib.acLibName, sizeof(HI_AE_LIB_NAME) + 1, HI_AE_LIB_NAME, sizeof(HI_AE_LIB_NAME));
			s32Ret = HI_MPI_AE_Register(ViPipe, &stAeLib);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("SAMPLE_COMM_ISP_Aelib_Callback failed\n");
                return s32Ret;
            }

			ALG_LIB_S stAwbLib;
			stAwbLib.s32Id = ViPipe;
			(HI_VOID)strncpy_s(stAwbLib.acLibName, sizeof(HI_AWB_LIB_NAME) + 1, HI_AWB_LIB_NAME, sizeof(HI_AWB_LIB_NAME));
			s32Ret = HI_MPI_AWB_Register(ViPipe, &stAwbLib);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("SAMPLE_COMM_ISP_Awblib_Callback failed\n");
                return s32Ret;
            }

            s32Ret = HI_MPI_ISP_MemInit(ViPipe);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("Init Ext memory failed with %#x!\n", s32Ret);
                return s32Ret;
            }

            s32Ret = HI_MPI_ISP_SetPubAttr(ViPipe, &stPubAttr);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("SetPubAttr failed with %#x!\n", s32Ret);
                return s32Ret;
            }

            s32Ret = HI_MPI_ISP_Init(ViPipe);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("ISP Init failed with %#x!\n", s32Ret);
                return s32Ret;
            }

            s32Ret = OS_COMM_ISP_Run(ViPipe);
            if (s32Ret != HI_SUCCESS) {
                OSALOG_ERROR("ISP Run failed with %#x!\n", s32Ret);;
                return s32Ret;
            }
        }
    }

    return s32Ret;
}

/*****************************************************************************
* function : init vi param
*****************************************************************************/
HI_S32 OS_COMM_VI_CreateIsp(os_vi_cfg *pstViConfig)
{
	HI_S32              i;
	HI_S32              s32ViNum;
	HI_S32              s32Ret = HI_FAILURE;
	os_vi_info_s       *pstViInfo = HI_NULL;

	if (pstViConfig == HI_NULL) {
	    OSALOG_ERROR("%s: null ptr\n", __FUNCTION__);
	    return s32Ret;
	}

	for (i = 0; i < pstViConfig->iWorkingViNum; i++) {
	    s32ViNum  = pstViConfig->aiWorkingViId[i];
	    pstViInfo = &pstViConfig->astViInfo[s32ViNum];
	    s32Ret = OS_COMM_VI_StartIsp(pstViInfo);
	    if (s32Ret != HI_SUCCESS) {
	        OSALOG_ERROR("SAMPLE_COMM_VI_StartIsp failed !\n");
	        return s32Ret;
	    }
	}

	return s32Ret;
}

static int OS_COMM_VI_StartVi(os_vi_cfg *pstViConfig)
{
    HI_S32 s32Ret = HI_FAILURE;

    if (pstViConfig == HI_NULL) {
        OSALOG_ERROR("null ptr\n");
        return s32Ret;
    }

	/* 1.根据sensor类型和硬件接线配置mipi*/
    s32Ret = OS_COMM_VI_StartMIPI(pstViConfig);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("SAMPLE_COMM_VI_StartMIPI failed!\n");
        return s32Ret;
    }

#if 0
	/* 2.配置VI工作模式 */
    s32Ret = OS_COMM_VI_SetParam(pstViConfig);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("SAMPLE_COMM_VI_SetParam failed!\n");
        return s32Ret;
    }
#endif
	/* 3.创建VI */
    s32Ret = OS_COMM_VI_CreateVi(pstViConfig);
    if (s32Ret != HI_SUCCESS) {
        OSALOG_ERROR("SAMPLE_COMM_VI_CreateVi failed!\n");
        return s32Ret;
    }

	/* 4.ISP配置 */
    s32Ret = OS_COMM_VI_CreateIsp(pstViConfig);
    if (s32Ret != HI_SUCCESS) {
        //SAMPLE_COMM_VI_DestroyVi(pstViConfig);
        OSALOG_ERROR("SAMPLE_COMM_VI_CreateIsp failed!\n");
        return s32Ret;
    }

    return AV_R_SOK;
}


HI_S32 OS_VI_Init(os_stream_ctx *ctx)
{
	HI_S32 s32Ret;
	os_vi_cfg *vicfg = &ctx->stViCfg;
	os_vb_cfg *vbcfg = &ctx->stVbCfg;
	
	/* 1. system init, vb alloc */	
	HI_U64 u64BlkSize;
	u64BlkSize = COMMON_GetPicBufferSize(vicfg->width, vicfg->height, PIXEL_FORMAT_YVU_SEMIPLANAR_422,
        DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
	vbcfg->stVbConfig.astCommPool[0].u32BlkCnt = DEFAULT_COMM_VB_NUM;
	vbcfg->stVbConfig.astCommPool[0].u64BlkSize = u64BlkSize;
	vbcfg->stVbConfig.u32MaxPoolCnt = 1;
	s32Ret = OS_COMM_SYS_Init(&vbcfg->stVbConfig);
	if (s32Ret != AV_R_SOK)
	{
		OSALOG_ERROR("OS_COMM_SYS_Init failed!\n");
		return s32Ret;
	}

	/* 2. vi init, vi pipe mode/vi chn set */
	s32Ret = OS_COMM_VI_SetParam(vicfg);
	if (s32Ret != AV_R_SOK)
	{
		OSALOG_ERROR("OS_COMM_VI_SetParam failed!\n");
		return s32Ret;
	}

	/* 3. isp param set */
	ISP_CTRL_PARAM_S stIspCtrlParam;
   	s32Ret = HI_MPI_ISP_GetCtrlParam(vicfg->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("HI_MPI_ISP_GetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl = vicfg->fps / 30; //isp统计信息更新帧频率

    s32Ret = HI_MPI_ISP_SetCtrlParam(vicfg->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("HI_MPI_ISP_SetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

	/* 4. start vi */
    s32Ret = OS_COMM_VI_StartVi(vicfg);
    if (HI_SUCCESS != s32Ret) {
        OS_COMM_SYS_Exit();
        OSALOG_ERROR("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        return s32Ret;
    }
	
	return AV_R_SOK;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

