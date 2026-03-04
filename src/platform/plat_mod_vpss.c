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
#include "hi_comm_video.h"
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_vpss.h"
#include "mpi_vpss.h"
#include "hi_buffer.h"

#include <plat_stream.h>
#include <plat_stream_priv.h>


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

HI_S32 OS_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL *pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,
    PIXEL_FORMAT_E enPixelFormat, SIZE_S stSize[], OS_SNS_TYPE_E enSnsType)
{
    HI_S32 i, j;
    HI_S32 s32Ret;
    SIZE_S stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = { 0 };
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    VPSS_CHN VpssChn;

	stSnsSize.u32Width  = 1920;
	stSnsSize.u32Height = 1080;
    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat = enPixelFormat;
    stVpssGrpAttr.u32MaxW = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
    for (i = 0; i < VPSS_MAX_PHY_CHN_NUM; i++) {
        if (HI_TRUE == pabChnEnable[i]) {
            stVpssChnAttr[i].u32Width = stSize[i].u32Width;
            stVpssChnAttr[i].u32Height = stSize[i].u32Height;
            stVpssChnAttr[i].enChnMode = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode = COMPRESS_MODE_NONE;
            stVpssChnAttr[i].enDynamicRange = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate = -1;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate = -1;
            stVpssChnAttr[i].u32Depth = 1;
            stVpssChnAttr[i].bMirror = HI_FALSE;
            stVpssChnAttr[i].bFlip = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode = ASPECT_RATIO_NONE;
        }
    }

    s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, &stVpssGrpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        OSALOG_ERROR("HI_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return HI_FAILURE;
    }
    for (j = 0; j < VPSS_MAX_PHY_CHN_NUM; j++)
    {
        if(HI_TRUE == pabChnEnable[j])
        {
            VpssChn = j;
            s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr[VpssChn]);
            if (s32Ret != HI_SUCCESS)
            {
                OSALOG_ERROR("HI_MPI_VPSS_SetChnAttr failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }

            s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);
            if (s32Ret != HI_SUCCESS)
            {
                OSALOG_ERROR("HI_MPI_VPSS_EnableChn failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        OSALOG_ERROR("HI_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}





#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


