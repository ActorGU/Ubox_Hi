#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>


#include "hi_common.h"
#include "sample_vio.h"
#include "mpi_sys.h"
/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VIO_Usage(char *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0)VI (Online) - VPSS(Online) - VO.\n");
    printf("\t 1)WDR(Offline)- VPSS(Offline) - VO. LDC+DIS+SPREAD.\n");
    printf("\t 2)Resolute Ratio Switch.\n");
    printf("\t 3)GDC - VPSS LowDelay.\n");
    printf("\t 4)Double WDR Pipe.\n");
    printf("\t 5)FPN Calibrate & Correction.\n");
    printf("\t 6)WDR Switch.\n");
    printf("\t 7)90/180/270/0/free Rotate.\n");
    printf("\t 8)UserPic.\n");
    printf("\t 9)VI-VPSS-VO(MIPI_TX).\n\n");

    printf("\t Hi3516DV300/Hi3559V200/Hi3556V200) vo HDMI output.\n");
    printf("\t Hi3516CV500) vo BT1120 output.\n");
    printf("\t If you have any questions, please look at readme.txt!\n");
    return;
}
/******************************************************************************
* function    : Sample_VIO_HDMI
* Description : vio hdmi
******************************************************************************/
void Sample_VIO_HDMI()
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_U32 u32VoIntfType = 0;
    HI_U32  u32ChipId;

    
    HI_MPI_SYS_GetChipId(&u32ChipId);

    if (HI3516C_V500 == u32ChipId)
    {
        u32VoIntfType = 1;
    }
    else
    {
        u32VoIntfType = 0;
    }
    SAMPLE_VIO_MsgInit();

    s32Ret = SAMPLE_VIO_ViOnlineVpssOnlineRoute(u32VoIntfType);
    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_vio exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_vio exit abnormally!\n");
    }

    SAMPLE_VIO_MsgExit();
    
    return 0;

}

/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 err1 = HI_FAILURE;
    HI_S32 err2 = HI_FAILURE;
    HI_S32 status;
    HI_S32 s32Index;
    HI_U32 u32VoIntfType = 0;
    HI_U32  u32ChipId;

    pthread_t thread_tcp;

#ifdef __HuaweiLite__
#else
    signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);
#endif


    HI_MPI_SYS_GetChipId(&u32ChipId);

    if (HI3516C_V500 == u32ChipId)
    {
        u32VoIntfType = 1;
    }
    else
    {
        u32VoIntfType = 0;
    }


    SAMPLE_VIO_MsgInit();

    s32Ret = SAMPLE_VIO_ViOnlineVpssOnlineRoute(u32VoIntfType);
    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("sample_vio exit success!\n");
    }
    else
    {
        SAMPLE_PRT("sample_vio exit abnormally!\n");
    }

    SAMPLE_VIO_MsgExit();


    err1 = pthread_create(&thread_tcp,NULL,tcpserver,NULL);
    if(err1!=0)
    {
            printf("thread1_create Failed:%s\n",strerror(errno));

    }
    else
    {
            printf("thread1_create success\n");
    }

    PAUSE();
    // while (1)
    // {
    //     /* code */
    // }
    

    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
