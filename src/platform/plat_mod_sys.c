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
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "hi_buffer.h"
#include "plat_stream_priv.h"


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif

/******************************************************************************
* function : vb exit & MPI system exit
******************************************************************************/
void OS_COMM_SYS_Exit(void)
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_ExitModCommPool(VB_UID_VDEC);
    HI_MPI_VB_Exit();
    return;
}

HI_S32 OS_COMM_SYS_Init(VB_CONFIG_S* pstVbConfig)
{
    HI_S32 s32Ret = HI_FAILURE;

    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    if (NULL == pstVbConfig)
    {
        OSALOG_ERROR("input parameter is null, it is invaild!\n");
        return AV_R_EFAIL;
    }

    s32Ret = HI_MPI_VB_SetConfig(pstVbConfig);
    if (HI_SUCCESS != s32Ret)
    {
        OSALOG_ERROR("HI_MPI_VB_SetConf failed!\n");
        return AV_R_EFAIL;
    }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret)
    {
        OSALOG_ERROR("HI_MPI_VB_Init failed!\n");
        return AV_R_EFAIL;
    }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret)
    {
        OSALOG_ERROR("HI_MPI_SYS_Init failed!\n");
        HI_MPI_VB_Exit();
        return AV_R_EFAIL;
    }

    return AV_R_SOK;
}

HI_S32 OS_VI_Bind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp)
{
	HI_S32 s32Ret;
	MPP_CHN_S stSrcChn;
	MPP_CHN_S stDestChn;

    stSrcChn.enModId   = HI_ID_VI;
    stSrcChn.s32DevId  = ViPipe;
    stSrcChn.s32ChnId  = ViChn;

    stDestChn.enModId  = HI_ID_VPSS;
    stDestChn.s32DevId = VpssGrp;
    stDestChn.s32ChnId = 0;

	s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        OSALOG_ERROR("HI_MPI_SYS_Bind(VI-VPSS) failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }
	
	OSALOG_INFO("VI(%d, %d) bind VPSS(%d, 0) success.\n", ViPipe, ViChn, VpssGrp);
	return s32Ret;
}

HI_S32 OS_COMM_VPSS_Bind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn)
{
	HI_S32 s32Ret;
	MPP_CHN_S stSrcChn;
	MPP_CHN_S stDestChn;

	stSrcChn.enModId   = HI_ID_VPSS;
	stSrcChn.s32DevId  = VpssGrp;
	stSrcChn.s32ChnId  = VpssChn;

	stDestChn.enModId  = HI_ID_VENC;
	stDestChn.s32DevId = 0;
	stDestChn.s32ChnId = VencChn;

	s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        OSALOG_ERROR("HI_MPI_SYS_Bind(VPSS(%d, %d)-VENC(%d) failed with %#x\n", s32Ret, VpssGrp, VpssChn, VencChn);
        return HI_FAILURE;
    }	
	return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


