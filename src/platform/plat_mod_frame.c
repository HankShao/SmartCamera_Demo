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
#include "hi_comm_video.h"
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "mpi_vpss.h"
#include "hi_buffer.h"

#include "plat_mpp.h"


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif

#define VPSS_GROUP_ID 0


static void SAMPLE_COMM_VDEC_SaveYUVFile_Linear8Bit(FILE* pfd, VIDEO_FRAME_S* pVBuf)
{
    HI_U8* pY_map = HI_NULL;
    HI_U8* pC_map = HI_NULL;
    unsigned int w, h;
    HI_U8* pMemContent = HI_NULL;
    HI_U8 *pTmpBuff = HI_NULL;
    HI_U64 phy_addr;
    HI_U32 s32Csize = 0;
    HI_U32 s32Ysize = 0;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight;

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        s32Ysize = pVBuf->u32Stride[0] * pVBuf->u32Height;
        u32UvHeight = pVBuf->u32Height / 2;
    }
    else if(PIXEL_FORMAT_YVU_SEMIPLANAR_422 == enPixelFormat)
    {
        s32Ysize = pVBuf->u32Stride[0] * pVBuf->u32Height;
        u32UvHeight = pVBuf->u32Height;
    }
    else if(PIXEL_FORMAT_YUV_400 == enPixelFormat)
    {
        s32Ysize = pVBuf->u32Stride[0] * pVBuf->u32Height;
        u32UvHeight = 0;
    }
    else
    {
        OSALOG_INFO("%s %d: This YUV format is not support!\n",__func__, __LINE__);
        return;
    }

    pTmpBuff = (HI_U8 *)malloc(pVBuf->u32Width / 2);
    if(HI_NULL == pTmpBuff)
    {
        OSALOG_ERROR("malloc pTmpBuff (size=%d) fail!!!\n",pVBuf->u32Stride[0]);
        return;
    }

    phy_addr = pVBuf->u64PhyAddr[0];

    pY_map = (HI_U8*) HI_MPI_SYS_Mmap(phy_addr, s32Ysize);
    if (HI_NULL == pY_map)
    {
        OSALOG_ERROR("HI_MPI_SYS_Mmap for pY_map fail!!\n");
        free(pTmpBuff);
        pTmpBuff = HI_NULL;
        return;
    }
    if (PIXEL_FORMAT_YUV_400 != enPixelFormat) {
        s32Csize = u32UvHeight * pVBuf->u32Stride[1];
        pC_map = (HI_U8*)HI_MPI_SYS_Mmap(pVBuf->u64PhyAddr[1], s32Csize);
        if (HI_NULL == pC_map)
        {
            OSALOG_ERROR("HI_MPI_SYS_Mmap for pC_map fail!!\n");
            free(pTmpBuff);
            pTmpBuff = HI_NULL;
            HI_MPI_SYS_Munmap(pY_map, s32Ysize);
            pY_map = HI_NULL;
            return;
        }
    }

    fprintf(stderr, "saving......Y......");
    fflush(stderr);
    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pY_map + h * pVBuf->u32Stride[0];
        fwrite(pMemContent, pVBuf->u32Width, 1, pfd);
    }

    if(PIXEL_FORMAT_YUV_400 != enPixelFormat)
    {
        fflush(pfd);
        fprintf(stderr, "U......");
        fflush(stderr);

        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pC_map + h * pVBuf->u32Stride[1];

            pMemContent += 1;

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                pTmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }
            fwrite(pTmpBuff, pVBuf->u32Width / 2, 1, pfd);
        }
        fflush(pfd);

        fprintf(stderr, "V......");
        fflush(stderr);
        for (h = 0; h < u32UvHeight; h++)
        {
            pMemContent = pC_map + h * pVBuf->u32Stride[1];

            for (w = 0; w < pVBuf->u32Width / 2; w++)
            {
                pTmpBuff[w] = *pMemContent;
                pMemContent += 2;
            }
            fwrite(pTmpBuff, pVBuf->u32Width / 2, 1, pfd);
        }
    }
    fflush(pfd);

    fprintf(stderr, "done!\n");
    fflush(stderr);
    free(pTmpBuff);
    pTmpBuff = HI_NULL;

    if (pC_map != HI_NULL) {
        HI_MPI_SYS_Munmap(pC_map, s32Csize);
        pC_map = HI_NULL;
    }

    HI_MPI_SYS_Munmap(pY_map, s32Ysize);
    pY_map = HI_NULL;

    return;
}


struct vpp_frame_priv{
	VPSS_CHN chn;
	HI_BOOL bMmap;
	uint8_t* virAddr[3];
	uint32_t planesize[3];
	VIDEO_FRAME_INFO_S stVideoFrame;
};

void vpp_release_frame(void *p)
{
	HI_S32 ret;
	struct vpp_frame_priv *priv = (struct vpp_frame_priv *)p;

	if (priv->bMmap == HI_TRUE)
	{
		HI_MPI_SYS_Munmap(priv->virAddr[0], priv->planesize[0]);
		HI_MPI_SYS_Munmap(priv->virAddr[1], priv->planesize[1]);
	}
	
	ret = HI_MPI_VPSS_ReleaseChnFrame(VPSS_GROUP_ID, priv->chn, &priv->stVideoFrame);
	if (ret != HI_SUCCESS)
	{
		OSALOG_ERROR("HI_MPI_VPSS_ReleaseChnFrame failed! ret = %d\n", ret);
	}

	return;
}

int32_t mpp_vpp_getframe(int32_t chn, vpp_frame_info_t *frame)
{
	HI_S32 s32Ret;
	HI_S32 s32MilliSec = 100;
	struct vpp_frame_priv *priv = malloc(sizeof(struct vpp_frame_priv));
	memset(priv, 0, sizeof(struct vpp_frame_priv));
	
	VIDEO_FRAME_S* pVframe = &priv->stVideoFrame.stVFrame;
	uint8_t* pVirAddrY = NULL;
	uint8_t* pVirAddrUC = NULL;
	uint32_t u32Ysize;
	uint32_t u32UvHeight;
	
	priv->chn = chn;	
	if ((s32Ret = HI_MPI_VPSS_GetChnFrame(VPSS_GROUP_ID, chn, &priv->stVideoFrame, s32MilliSec)) != HI_SUCCESS)
	{
		OSALOG_ERROR("Get frame from VPSS fail(0x%x)!\n", s32Ret);
		return AV_R_EFAIL;
	}

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pVframe->enPixelFormat)
    {
        u32Ysize = pVframe->u32Stride[0] * pVframe->u32Height;
        u32UvHeight = pVframe->u32Height / 2;

		if (pVframe->u64VirAddr[0] == HI_NULL)
		{
			pVirAddrY = HI_MPI_SYS_Mmap(pVframe->u64PhyAddr[0], u32Ysize);
			if (HI_NULL == pVirAddrY)
			{
				OSALOG_ERROR("HI_MPI_SYS_Mmap for pY_map fail!!\n");
				return AV_R_EFAIL;
			}
			pVirAddrUC = HI_MPI_SYS_Mmap(pVframe->u64PhyAddr[1], u32UvHeight * pVframe->u32Stride[1]);
			if (HI_NULL == pVirAddrUC)
			{
				OSALOG_ERROR("HI_MPI_SYS_Mmap for pY_map fail!!\n");
				return AV_R_EFAIL;
			}

			priv->virAddr[0] = pVirAddrY;
			priv->planesize[0] = u32Ysize;
			priv->virAddr[1] = pVirAddrUC;
			priv->planesize[1] = u32UvHeight * pVframe->u32Stride[1];
			priv->bMmap = HI_TRUE;
		}

    }

#if 0
	if (0 != access("./1.yuv", F_OK))
    {
		FILE *fp = fopen("./1.yuv", "w+");
		fwrite(priv->virAddr[0], 1, priv->planesize[0], fp);
		fwrite(priv->virAddr[1], 1, priv->planesize[1], fp);
		fflush(fp);
		fclose(fp);
	}
#endif

	frame->frame.width = priv->stVideoFrame.stVFrame.u32Width;
	frame->frame.height = priv->stVideoFrame.stVFrame.u32Height;
	memcpy(frame->frame.data, priv->virAddr, sizeof(priv->virAddr));
	memcpy(frame->frame.stride, pVframe->u32Stride, sizeof(pVframe->u32Stride));
	frame->frame.size[0] = u32Ysize;
	frame->frame.size[1] = u32UvHeight * pVframe->u32Stride[1];
	frame->priv = priv;
	frame->free = (void *)vpp_release_frame;

	frame->frame.data[0] = malloc(u32UvHeight * pVframe->u32Stride[1] + u32Ysize); 
	frame->frame.data[1] = frame->frame.data[0] + u32Ysize;
	memcpy(frame->frame.data[0], priv->virAddr[0], u32Ysize);
	memcpy(frame->frame.data[1], priv->virAddr[1], u32Ysize/2);

	
out:
	return s32Ret;
}







#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif



