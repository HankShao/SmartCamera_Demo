#ifndef __OSA_MACRO_H__
#define __OSA_MACRO_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdint.h>
/*
* 返回值定义区
*/
#define AV_R_SOK           0
#define AV_R_EFAIL        -1
#define AV_R_TIMEOUT      -2
#define AV_R_ILLEGALPARAM -3
#define AV_R_NULLPTR      -4
#define AV_R_NOTSUPPORT   -5
#define AV_R_NOMEM        -7


#define AV_TRUE   1
#define AV_FALSE  0

#define HANDLE    void*
typedef uint8_t   BOOL;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif
