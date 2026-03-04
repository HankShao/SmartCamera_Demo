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
#include "hi_buffer.h"

#include "plat_mpp.h"


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif

int32_t mpp_encode_jpg(void *frame, jpgenc_info_t *out)
{
	

	return AV_R_SOK;
}







#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


