#ifndef __PLAT_MPP_PRIV_H__
#define __PLAT_MPP_PRIV_H__



#define VPSS_GROUP_ID 0
#define JPEG_CHN_ID   4

struct vpp_frame_priv{
	VPSS_CHN chn;
	HI_BOOL bUsedTDE;
	HI_BOOL bUsedVGS;
	HI_BOOL bMmap;
	VB_BLK hBlk;
	uint8_t* virAddr[3];
	uint32_t planesize[3];
	VIDEO_FRAME_INFO_S stVideoFrame;
};

#endif

