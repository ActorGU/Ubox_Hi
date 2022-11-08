/******************************************************************************

  Copyright (C), 2017, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name     : sample_venc.c
  Version       : Initial Draft
  Author        : Hisilicon multimedia software group
  Created       : 2017
  Description   :
******************************************************************************/

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
#include <errno.h>
#include <netinet/in.h>

#include "sample_comm.h"
#include "rtsp_demo.h"
#include "uart.h"
#include "uart_init.h"
#include <sys/prctl.h>
#include <fcntl.h>
extern HI_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char* szFilePostfix);

rtsp_demo_handle g_rtsplive = NULL;
rtsp_session_handle session = NULL;
HI_BOOL save_orgimg = HI_FALSE;
#ifdef HI_FPGA
    #define PIC_SIZE   PIC_1080P
#else
    #define PIC_SIZE   PIC_1080P
#endif

#define PORT 8080
HI_S32 sock_fd, client_fd;
pthread_t thread_tcp;
pthread_t thread_saveraw;
// HI_VOID* SaveRaw_Inquire( HI_VOID* arga );
// HI_VOID* tcpserver( HI_VOID* arg );
/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VENC_Usage(char* sPrgNm)
{
    printf("Usage : %s [index] \n", sPrgNm);
    printf("index:\n");
    printf("\t  0) H.265e + H264e.\n");
    printf("\t  1) Lowdelay:H.265e.\n");
    printf("\t  2) Qpmap:H.265e + H264e.\n");
    printf("\t  3) IntraRefresh:H.265e + H264e.\n");
    printf("\t  4) RoiBgFrameRate:H.265e + H.264e.\n");
    printf("\t  5) DeBreathEffect:H.265e + H.264e.\n");
    printf("\t  6) svc-t :H.264.\n");
    printf("\t  7) Mjpeg +Jpeg.\n");

    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VENC_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_VENC_StopSendQpmapFrame();
        SAMPLE_COMM_VENC_StopGetStream();
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    if(session)
    {
        rtsp_del_session(session);
        printf("rtsp_del_session.\n");
    }
		
	if(g_rtsplive)
    {
        rtsp_del_demo(g_rtsplive);
        printf("rtsp_del_demo.\n");
    }
		
    
    exit(-1);
}

/******************************************************************************
* function : to process abnormal case - the case of stream venc
******************************************************************************/
void SAMPLE_VENC_StreamHandleSig(HI_S32 signo)
{

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}

VENC_GOP_MODE_E SAMPLE_VENC_GetGopMode(void)
{
    char c;
    VENC_GOP_MODE_E enGopMode = 0;

Begin_Get:

    printf("please input choose gop mode!\n");
    printf("\t 0) NORMALP.\n");
    printf("\t 1) DUALP.\n");
    printf("\t 2) SMARTP.\n");

    while((c = getchar()) != '\n' && c != EOF)
    switch(c)
    {
        case '0':
            enGopMode = VENC_GOPMODE_NORMALP;
            break;
        case '1':
            enGopMode = VENC_GOPMODE_DUALP;
            break;
        case '2':
            enGopMode = VENC_GOPMODE_SMARTP;
            break;
        default:
            SAMPLE_PRT("input rcmode: %c, is invaild!\n",c);
            goto Begin_Get;
    }

    return enGopMode;
}

SAMPLE_RC_E SAMPLE_VENC_GetRcMode(void)
{
    char c;
    SAMPLE_RC_E  enRcMode = 0;

Begin_Get:

    printf("please input choose rc mode!\n");
    printf("\t c) cbr.\n");
    printf("\t v) vbr.\n");
    printf("\t a) avbr.\n");
    printf("\t x) cvbr.\n");
    printf("\t q) qvbr.\n");
    printf("\t f) fixQp\n");

    while((c = getchar()) != '\n' && c != EOF)
    switch(c)
    {
        case 'c':
            enRcMode = SAMPLE_RC_CBR;
            break;
        case 'v':
            enRcMode = SAMPLE_RC_VBR;
            break;
        case 'a':
            enRcMode = SAMPLE_RC_AVBR;
            break;
        case 'q':
            enRcMode = SAMPLE_RC_QVBR;
            break;
        case 'x':
            enRcMode = SAMPLE_RC_CVBR;
            break;
        case 'f':
            enRcMode = SAMPLE_RC_FIXQP;
            break;
        default:
            SAMPLE_PRT("input rcmode: %c, is invaild!\n",c);
            goto Begin_Get;
    }
    return enRcMode;
}

VENC_INTRA_REFRESH_MODE_E SAMPLE_VENC_GetIntraRefreshMode(void)
{
    char c;
    VENC_INTRA_REFRESH_MODE_E   enIntraRefreshMode = INTRA_REFRESH_ROW;

Begin_Get:

    printf("please input choose IntraRefresh mode!\n");
    printf("\t r) ROW.\n");
    printf("\t c) COLUMN.\n");

    while((c = getchar()) != '\n' && c != EOF)
    switch(c)
    {
        case 'r':
            enIntraRefreshMode = INTRA_REFRESH_ROW;
            break;
        case 'c':
            enIntraRefreshMode = INTRA_REFRESH_COLUMN;
            break;

        default:
            SAMPLE_PRT("input IntraRefresh Mode: %c, is invaild!\n",c);
            goto Begin_Get;
    }
    return enIntraRefreshMode;
}


HI_S32 SAMPLE_VENC_SYS_Init(HI_U32 u32SupplementConfig,SAMPLE_SNS_TYPE_E  enSnsType)
{
    HI_S32 s32Ret;
    HI_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;
    PIC_SIZE_E enSnsSize;
    SIZE_S     stSnsSize;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    u64BlkSize = COMMON_GetPicBufferSize(stSnsSize.u32Width, stSnsSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_NONE,DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize   = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt    = 20;

    // u64BlkSize = COMMON_GetPicBufferSize(2048, 2048, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_12, COMPRESS_MODE_NONE,DEFAULT_ALIGN);
    // stVbConf.astCommPool[1].u64BlkSize   = u64BlkSize;
    // stVbConf.astCommPool[1].u32BlkCnt    = 20;

    stVbConf.u32MaxPoolCnt = 1;

    if(0 == u32SupplementConfig)
    {
        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    }
    else
    {
        s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf,u32SupplementConfig);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_VOID SAMPLE_VENC_SetDCFInfo(VI_PIPE ViPipe)
{
    ISP_DCF_INFO_S stIspDCF;

    HI_MPI_ISP_GetDCFInfo(ViPipe, &stIspDCF);

    //description: Thumbnail test
    strncpy((char *)stIspDCF.stIspDCFConstInfo.au8ImageDescription,"Thumbnail test",DCF_DRSCRIPTION_LENGTH);
    //manufacturer: Hisilicon
    strncpy((char *)stIspDCF.stIspDCFConstInfo.au8Make,"Hisilicon",DCF_DRSCRIPTION_LENGTH);
    //model number: Hisilicon IP Camera
    strncpy((char *)stIspDCF.stIspDCFConstInfo.au8Model,"Hisilicon IP Camera",DCF_DRSCRIPTION_LENGTH);
    //firmware version: v.1.1.0
    strncpy((char *)stIspDCF.stIspDCFConstInfo.au8Software,"v.1.1.0",DCF_DRSCRIPTION_LENGTH);


    stIspDCF.stIspDCFConstInfo.u32FocalLength             = 0x00640001;
    stIspDCF.stIspDCFConstInfo.u8Contrast                 = 5;
    stIspDCF.stIspDCFConstInfo.u8CustomRendered           = 0;
    stIspDCF.stIspDCFConstInfo.u8FocalLengthIn35mmFilm    = 1;
    stIspDCF.stIspDCFConstInfo.u8GainControl              = 1;
    stIspDCF.stIspDCFConstInfo.u8LightSource              = 1;
    stIspDCF.stIspDCFConstInfo.u8MeteringMode             = 1;
    stIspDCF.stIspDCFConstInfo.u8Saturation               = 1;
    stIspDCF.stIspDCFConstInfo.u8SceneCaptureType         = 1;
    stIspDCF.stIspDCFConstInfo.u8SceneType                = 0;
    stIspDCF.stIspDCFConstInfo.u8Sharpness                = 5;
    stIspDCF.stIspDCFUpdateInfo.u32ISOSpeedRatings         = 500;
    stIspDCF.stIspDCFUpdateInfo.u32ExposureBiasValue       = 5;
    stIspDCF.stIspDCFUpdateInfo.u32ExposureTime            = 0x00010004;
    stIspDCF.stIspDCFUpdateInfo.u32FNumber                 = 0x0001000f;
    stIspDCF.stIspDCFUpdateInfo.u8WhiteBalance             = 1;
    stIspDCF.stIspDCFUpdateInfo.u8ExposureMode             = 0;
    stIspDCF.stIspDCFUpdateInfo.u8ExposureProgram          = 1;
    stIspDCF.stIspDCFUpdateInfo.u32MaxApertureValue        = 0x00010001;

    HI_MPI_ISP_SetDCFInfo(ViPipe, &stIspDCF);

    return;
}

HI_S32 SAMPLE_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig)
{
    HI_S32              s32Ret;
    SAMPLE_SNS_TYPE_E   enSnsType;
    ISP_CTRL_PARAM_S    stIspCtrlParam;
    HI_U32              u32FrameRate;


    enSnsType = pstViConfig->astViInfo[0].stSnsInfo.enSnsType;//SENSOR0_TYPE
    pstViConfig->s32WorkingViNum                              = 1;
    pstViConfig->as32WorkingViId[0]                           = 0;
    pstViConfig->astViInfo[0].stSnsInfo.MipiDev               = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);
    pstViConfig->astViInfo[0].stSnsInfo.s32BusId              = 0;
    pstViConfig->astViInfo[0].stDevInfo.ViDev                 = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);
    pstViConfig->astViInfo[0].stDevInfo.enWDRMode             = WDR_MODE_NONE;

    if(HI_TRUE == bLowDelay)
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_ONLINE_VPSS_ONLINE;
    }
    else
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_OFFLINE_VPSS_OFFLINE;
    }
    //VB init
    s32Ret = SAMPLE_VENC_SYS_Init(u32SupplementConfig,enSnsType);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Init SYS err for %#x!\n", s32Ret);
        return s32Ret;
    }
    // memset(&stVbConf, 0, sizeof(VB_CONFIG_S));//缓存池内存清0
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[0]          = 0;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    pstViConfig->astViInfo[0].stChnInfo.ViChn              = 0;
    pstViConfig->astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pstViConfig->astViInfo[0].stChnInfo.enDynamicRange     = DYNAMIC_RANGE_SDR8;
    pstViConfig->astViInfo[0].stChnInfo.enVideoFormat      = VIDEO_FORMAT_LINEAR;
    pstViConfig->astViInfo[0].stChnInfo.enCompressMode     = COMPRESS_MODE_NONE;
    s32Ret = SAMPLE_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl  = u32FrameRate/25;

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,PIXEL_FORMAT_E enPixelFormat,SIZE_S stSize[],SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 i;
    HI_S32 s32Ret;
    PIC_SIZE_E      enSnsSize;
    SIZE_S          stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];


    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }


    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat  = enPixelFormat;
    stVpssGrpAttr.u32MaxW        = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH        = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_FALSE;
    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_NONE;


    for(i=0; i<VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if(HI_TRUE == pabChnEnable[i])
        {
            stVpssChnAttr[i].u32Width                     = stSize[i].u32Width;
            stVpssChnAttr[i].u32Height                    = stSize[i].u32Height;
            stVpssChnAttr[i].enChnMode                    = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode               = COMPRESS_MODE_NONE;//COMPRESS_MODE_SEG;
            stVpssChnAttr[i].enDynamicRange               = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat                = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate  = -1;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate  = -1;
            stVpssChnAttr[i].u32Depth                     = 5;
            stVpssChnAttr[i].bMirror                      = HI_FALSE;
            stVpssChnAttr[i].bFlip                        = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat                = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode         = ASPECT_RATIO_NONE;
        }
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, pabChnEnable,&stVpssGrpAttr,stVpssChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
    }



    return s32Ret;
}


HI_S32 SAMPLE_VENC_CheckSensor(SAMPLE_SNS_TYPE_E   enSnsType,SIZE_S  stSize)
{
    HI_S32 s32Ret;
    SIZE_S          stSnsSize;
    PIC_SIZE_E      enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    if((stSnsSize.u32Width < stSize.u32Width) || (stSnsSize.u32Height < stSize.u32Height))
    {
        //SAMPLE_PRT("Sensor size is (%d,%d), but encode chnl is (%d,%d) !\n",
        //    stSnsSize.u32Width,stSnsSize.u32Height,stSize.u32Width,stSize.u32Height);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_VENC_ModifyResolution(SAMPLE_SNS_TYPE_E   enSnsType,PIC_SIZE_E *penSize,SIZE_S *pstSize)
{
    HI_S32 s32Ret;
    SIZE_S          stSnsSize;
    PIC_SIZE_E      enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    *penSize = enSnsSize;
    pstSize->u32Width  = stSnsSize.u32Width;
    pstSize->u32Height = stSnsSize.u32Height;

    return HI_SUCCESS;
}
HI_S32 CY_ISP_PARAM_SET()
{
    HI_S32 ret;
    VI_PIPE ViPipe = 0;

    // Module Bypass
    ISP_MODULE_CTRL_U unModCtrl; 
    ret = HI_MPI_ISP_GetModuleControl(ViPipe, &unModCtrl);
    if ( HI_SUCCESS == ret ) {
        unModCtrl.bitBypassAEStatBE = HI_FALSE;
        unModCtrl.bitBypassAEStatFE = HI_FALSE;
        unModCtrl.bitBypassAFStatBE = HI_TRUE;
        unModCtrl.bitBypassAFStatFE = HI_TRUE;
        unModCtrl.bitBypassAntiFC = HI_TRUE;
        unModCtrl.bitBypassAWBStat = HI_TRUE;
        unModCtrl.bitBypassCA = HI_TRUE;
        unModCtrl.bitBypassCLUT = HI_TRUE;
        unModCtrl.bitBypassColorMatrix = HI_TRUE;
        unModCtrl.bitBypassCrosstalkR = HI_TRUE;
        unModCtrl.bitBypassCsConv = HI_FALSE;
        unModCtrl.bitBypassDE = HI_TRUE;
        unModCtrl.bitBypassDehaze = HI_FALSE;
        unModCtrl.bitBypassDemosaic = HI_TRUE;
        unModCtrl.bitBypassDPC = HI_FALSE;
        unModCtrl.bitBypassDRC = HI_TRUE;
        unModCtrl.bitBypassEdgeMark = HI_TRUE;
        unModCtrl.bitBypassFSWDR = HI_TRUE;
        unModCtrl.bitBypassGamma = HI_FALSE;
        unModCtrl.bitBypassGCAC = HI_TRUE;
        unModCtrl.bitBypassHLC = HI_TRUE;
        unModCtrl.bitBypassISPDGain = HI_FALSE;
        unModCtrl.bitBypassLCAC = HI_TRUE;
        unModCtrl.bitBypassLdci = HI_FALSE;
        unModCtrl.bitBypassMeshShading = HI_TRUE;
        unModCtrl.bitBypassMGStat = HI_TRUE;
        unModCtrl.bitBypassNR = HI_FALSE;
        unModCtrl.bitBypassPreGamma = HI_TRUE;
        unModCtrl.bitBypassRadialCrop = HI_TRUE;
        unModCtrl.bitBypassRadialShading = HI_TRUE;
        unModCtrl.bitBypassRGBIR = HI_TRUE;
        unModCtrl.bitBypassSharpen = HI_FALSE;
        unModCtrl.bitBypassWBGain = HI_TRUE;
        ret = HI_MPI_ISP_SetModuleControl(ViPipe, &unModCtrl);
        if (HI_SUCCESS != ret)
        {
            SAMPLE_PRT("HI_MPI_ISP_SetModuleControl!\n");
            return ret;
        }
    }

       // LDCI
    ISP_LDCI_ATTR_S stLDCIAttr; 
    ret = HI_MPI_ISP_GetLDCIAttr(ViPipe, &stLDCIAttr);
    if ( HI_SUCCESS == ret ) {
        stLDCIAttr.bEnable = HI_TRUE;
        stLDCIAttr.enOpType = OP_TYPE_MANUAL;
        stLDCIAttr.stManual.u16BlcCtrl = 5;
        stLDCIAttr.stManual.stHeWgt.stHePosWgt.u8Wgt = 64;
        stLDCIAttr.stManual.stHeWgt.stHePosWgt.u8Sigma = 80;
        stLDCIAttr.stManual.stHeWgt.stHePosWgt.u8Mean =0;
        stLDCIAttr.stManual.stHeWgt.stHeNegWgt.u8Wgt = 69;
        stLDCIAttr.stManual.stHeWgt.stHeNegWgt.u8Sigma = 80;
        stLDCIAttr.stManual.stHeWgt.stHeNegWgt.u8Mean = 128;
        stLDCIAttr.u8GaussLPFSigma = 36;
        stLDCIAttr.u16TprDecrCoef = 256;
        stLDCIAttr.u16TprIncrCoef = 256;        
        ret = HI_MPI_ISP_SetLDCIAttr(ViPipe, &stLDCIAttr);
        if (HI_SUCCESS != ret)
        {
            SAMPLE_PRT("HI_MPI_ISP_SetLDCIAttr!\n");
            return ret;
        }
    }

     // Sharpen
    ISP_SHARPEN_ATTR_S  stIspShpAttr; 
    ret = HI_MPI_ISP_GetIspSharpenAttr(ViPipe, &stIspShpAttr);
    if ( HI_SUCCESS == ret ) {
        stIspShpAttr.bEnable = HI_TRUE;
        stIspShpAttr.enOpType = OP_TYPE_MANUAL;
        stIspShpAttr.stManual.u8OverShoot = 127;
         stIspShpAttr.stManual.u8UnderShoot =127;
         stIspShpAttr.stManual.u8DetailCtrl = 128;
         stIspShpAttr.stManual.u8DetailCtrlThr =160;
         stIspShpAttr.stManual.u8EdgeFiltStr = 63;
         stIspShpAttr.stManual.u8EdgeFiltMaxCap = 47;
         stIspShpAttr.stManual.u8RGain =0;
         stIspShpAttr.stManual.u8GGain =0;
         stIspShpAttr.stManual.u8BGain =0;
         stIspShpAttr.stManual.u8SkinGain =0;
        ret = HI_MPI_ISP_SetIspSharpenAttr(ViPipe, &stIspShpAttr);
        if (HI_SUCCESS != ret)
        {
            SAMPLE_PRT("HI_MPI_ISP_SetIspSharpenAttr!\n");
            return ret;
        }
    }
         // Dehaze
    ISP_DEHAZE_ATTR_S stDehazeAttr; 
    ret = HI_MPI_ISP_GetDehazeAttr(ViPipe, &stDehazeAttr);
    if ( HI_SUCCESS == ret ) {
        stDehazeAttr.bEnable = HI_TRUE;
        stDehazeAttr.bUserLutEnable = HI_FALSE;
        stDehazeAttr.enOpType = OP_TYPE_AUTO;
        stDehazeAttr.stManual.u8strength = 255;
        stDehazeAttr.stAuto.u8strength =90;
        stDehazeAttr.u16TmprfltDecrCoef = 128;
        stDehazeAttr.u16TmprfltIncrCoef = 128;
        ret = HI_MPI_ISP_SetDehazeAttr(ViPipe, &stDehazeAttr);
        if (HI_SUCCESS != ret)
        {
            SAMPLE_PRT("HI_MPI_ISP_SetDehazeAttr!\n");
            return ret;
        }
    }

        // Noise Reduce
    ISP_NR_ATTR_S stNRAttr; 
    ret = HI_MPI_ISP_GetNRAttr(ViPipe, &stNRAttr);
    if ( HI_SUCCESS == ret ) {
        stNRAttr.bEnable = HI_TRUE;
        stNRAttr.bNrLscEnable = HI_FALSE;
        stNRAttr.u8BnrLscMaxGain = 96;
        stNRAttr.u16BnrLscCmpStrength = 256;
        stNRAttr.enOpType = OP_TYPE_AUTO;
        ret = HI_MPI_ISP_SetNRAttr(ViPipe, &stNRAttr);
        if (HI_SUCCESS != ret)
        {
            SAMPLE_PRT("HI_MPI_ISP_SetNRAttr!\n");
            return ret;
        }
    }
    return HI_SUCCESS;
}

void get_frame_bytes(VIDEO_FRAME_INFO_S stFrmInfo,HI_U8* buf,int len)
{
    HI_U8* pVBufVirt_Y = NULL;
    pVBufVirt_Y = (HI_U8*) HI_MPI_SYS_Mmap(stFrmInfo.stVFrame.u64PhyAddr[0], len);
    if (HI_NULL == pVBufVirt_Y)
    {
        printf("get_frame_bytes mmap err.\n");
    }
    memcpy((void*)buf,pVBufVirt_Y,len);
            
    HI_MPI_SYS_Munmap(pVBufVirt_Y, len);
    pVBufVirt_Y = NULL;
}


void* Hi_tcpserver( void* argc)
{
    // HI_S32 sock_fd, client_fd;
    HI_S32 addr_client_len;
    struct sockaddr_in addr_server, addr_client;
    HI_S32 iDataNum;
	HI_U8 buffer[200];

    addr_client_len = sizeof(struct sockaddr);
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    printf("sock successful\n");
 
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(PORT);
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
 
    if(bind(sock_fd, (struct sockaddr *)&addr_server, sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
        exit(1);
    }
 
    printf("bind sucessful\n");
 
    if(listen(sock_fd, 5))
    {
        perror("listen");
        exit(1);
    }
    printf("listen sucessful\n");
	
    // while(1)
    // {
        if((client_fd = accept(sock_fd, (struct sockaddr *)&addr_client, &addr_client_len)) < 0)
        {
            perror("accept");
            exit(1);		
        }
        printf("accept client ip: %s\n", inet_ntoa(addr_client.sin_addr));
        printf("Port is %d\n",htons(addr_client.sin_port));
        while(1)
        {
            iDataNum = recv(client_fd,buffer,1024,0);
            if(iDataNum < 0)
            {
                perror("recv");
                continue;
            }
            buffer[iDataNum] = '\0';
            // if(strcmp(buffer,"quit") == 0)
            // {
            //     close(sock_fd);
            //     close(client_fd);
            //     break;
            // }
            if(strcmp(buffer,"save") == 0)
            {
                // send(client_fd, "raw is saving...", 14, 0);
                save_orgimg = HI_TRUE;
            }
            else
            {
                // printf("%drecv data is %s\n",iDataNum,buffer);
                send(client_fd, buffer, iDataNum, 0);
            }

        }

    usleep(1000);
    return 0;
}


//TODO
HI_VOID* SAMPLE_COMM_VENC_GetVencStreamProc_rtsp(HI_VOID* p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S* pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_U32 u32PictureCnt[VENC_MAX_CHN_NUM]={0};
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE* pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

    prctl(PR_SET_NAME, "GetVencStream", 0,0,0);

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    s32ChnTotal = pstPara->s32Cnt;
    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        VencChn = pstPara->VeChn[i];
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
                       VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
                       stVencChnAttr.stVencAttr.enType, s32Ret);
            return NULL;
        }
        if(PT_JPEG != enPayLoadType[i])
        {
            snprintf(aszFileName[i],32, "stream_chn%d%s", i, szFilePostfix);

            pFile[i] = fopen(aszFileName[i], "wb");
            if (!pFile[i])
            {
                SAMPLE_PRT("open file[%s] failed!\n",
                           aszFileName[i]);
                return NULL;
            }
        }
        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n",
                       VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }

        s32Ret = HI_MPI_VENC_GetStreamBufInfo (i, &stStreamBufInfo[i]);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return (void *)HI_FAILURE;
        }
    }
    unsigned char* pStremData = NULL;
    int nSize = 0; 
	g_rtsplive = create_rtsp_demo(554);
	session= create_rtsp_session(g_rtsplive,"/live.sdp");
    printf("rtsp://192.168.3.32/live.sdp\n");
    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 5;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            for (i = 0; i < s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));

                    s32Ret = HI_MPI_VENC_QueryStatus(i, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    /*******************************************************
                    step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                     if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                     }
                    *******************************************************/
                    if(0 == stStat.u32CurPacks)
                    {
                          SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                          continue;
                    }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
                                   s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    if(PT_JPEG == enPayLoadType[i])
                    {
                        snprintf(aszFileName[i],32, "stream_chn%d_%d%s", i, u32PictureCnt[i],szFilePostfix);
                        pFile[i] = fopen(aszFileName[i], "wb");
                        if (!pFile[i])
                        {
                            SAMPLE_PRT("open file err!\n");
                            return NULL;
                        }
                    }

#ifndef __HuaweiLite__
                    // 向FIFO文件写数据                  
                    for (int x = 0; x < stStream.u32PackCount; x++)
                    {                          
                        pStremData = (unsigned char*)stStream.pstPack[x].pu8Addr + stStream.pstPack[x].u32Offset;
                        nSize = stStream.pstPack[x].u32Len - stStream.pstPack[x].u32Offset;           
                        if(g_rtsplive)
                        {
                            rtsp_sever_tx_video(g_rtsplive,session,pStremData,nSize,stStream.pstPack[x].u64PTS);
                        }                 
                    }
#else
                    s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile[i], &stStreamBufInfo[i], &stStream);
#endif
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
                    /*******************************************************
                     step 2.6 : release stream
                     *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    u32PictureCnt[i]++;
                    if(PT_JPEG == enPayLoadType[i])
                    {
                        fclose(pFile[i]);
                    }
                }
            }
        }
    }

    /*******************************************************
    * step 3 : close save-file
    *******************************************************/
    for (i = 0; i < s32ChnTotal; i++)
    {
        if(PT_JPEG != enPayLoadType[i])
        {
            fclose(pFile[i]);
        }
    }
    return NULL;
}

SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
pthread_t gs_VencPid;

//run RTSP
HI_S32 SAMPLE_COMM_VENC_StartGetStream_rtsp(VENC_CHN VeChn[],HI_S32 s32Cnt)
{
    HI_U32 i;
    
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;
    for(i=0; i<s32Cnt; i++)
    {
        gs_stPara.VeChn[i] = VeChn[i];
    }
    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc_rtsp, (HI_VOID*)&gs_stPara);
    // SAMPLE_COMM_VENC_GetVencStreamProc_rtsp((HI_VOID*)&gs_stPara);
    // return 0;
}
HI_VOID* Hi_SaveRaw( HI_VOID* argc )
{
    HI_S32 s32Ret;
    HI_U32 u32Size_b = 0;
    VIDEO_FRAME_INFO_S stBaseFrmInfo;
    VI_DUMP_ATTR_S    astBackUpDumpAttr;
    VI_DUMP_ATTR_S    stDumpAttr;   
    HI_U8 read_buf_ready[15] = {0};

    s32Ret = HI_MPI_VI_SetPipeFrameSource(0,VI_PIPE_FRAME_SOURCE_DEV);
    s32Ret = HI_MPI_VI_GetPipeDumpAttr(0, &astBackUpDumpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("Get Pipe %d dump attr failed!\n", 0);
        return HI_NULL;
    }
    memcpy(&stDumpAttr, &astBackUpDumpAttr, sizeof(VI_DUMP_ATTR_S));
    stDumpAttr.bEnable  = HI_TRUE;
    stDumpAttr.u32Depth = 2;
    s32Ret = HI_MPI_VI_SetPipeDumpAttr(0, &stDumpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        printf("Set Pipe %d dump attr failed!\n", 0);
        return HI_NULL;
    }
    while (1)
    {
        /* code */
        if(save_orgimg == HI_TRUE)
        {
            // for(int i = 0 ; i < 500 ; i++ )
            // {
                char bgrname[64];
                char PackegNum[32];
                static int imgfile_index=1;
                static char imgfile_index_char;
                sprintf(bgrname,"img/%04d",imgfile_index);
                strcat(bgrname,"-orgimg.raw");

                s32Ret = HI_MPI_VI_GetPipeFrame(0, &stBaseFrmInfo, 100);
                if(HI_SUCCESS != s32Ret)
                {
                    s32Ret = HI_MPI_VI_ReleasePipeFrame(0, &stBaseFrmInfo);
                    printf("get channel frame fail!");
                    printf("%x\n",s32Ret);
                }
                // get_frame_bytes(stBaseFrmInfo,read_buf_ready,10);//帧头10个字节
                // for(int i = 0 ; i < 10 ; i++ )
                // {
                //     printf("%x",read_buf_ready[i]);
                // }
                //debug end
                u32Size_b = stBaseFrmInfo.stVFrame.u32Stride[0]*stBaseFrmInfo.stVFrame.u32Height;
                HI_U8* pVBufVirt_Y8 = (HI_U8*) HI_MPI_SYS_Mmap(stBaseFrmInfo.stVFrame.u64PhyAddr[0], u32Size_b);
                if (NULL == pVBufVirt_Y8)
                {
                    SAMPLE_PRT("HI_MPI_SYS_Mmap fail !\n");
                    goto EXIT_SAVE_STOP;
                }
                FILE* pfile_bgr = fopen(bgrname, "wb+"); 
                if(pfile_bgr != NULL)
                {
                    printf("fopen successful!");
                }

                for(int h=0;h<stBaseFrmInfo.stVFrame.u32Height;h++)
                {
                    fwrite(pVBufVirt_Y8 + stBaseFrmInfo.stVFrame.u32Stride[0]*h, stBaseFrmInfo.stVFrame.u32Width, 1, pfile_bgr);                     
                }
                fflush(pfile_bgr); 
                fclose(pfile_bgr);
                s32Ret = HI_MPI_SYS_Munmap(pVBufVirt_Y8, u32Size_b); 
                if(HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("save_orgimg HI_MPI_SYS_Munmap failed, s32Ret:0x%x\n",s32Ret);
                    goto EXIT_SAVE_STOP;
                }             
                printf("save %s\n",bgrname);
                sprintf(PackegNum,"%03d Save Done",imgfile_index);
                send(client_fd, PackegNum ,13,0);
                imgfile_index++;
                // //debug
                s32Ret = HI_MPI_VI_ReleasePipeFrame (0,&stBaseFrmInfo);
                // s32Ret = HI_MPI_VPSS_ReleaseChnFrame(0,0,&stBaseFrmInfo);
                if(HI_SUCCESS != s32Ret)
                {
                    printf("Release channel frame failed!\n");
                    printf("%x\n",s32Ret);
                }
            // }
            save_orgimg = HI_FALSE;

        }

    usleep(1000);
    }
    
    
    return 0;

    EXIT_SAVE_STOP:
    SAMPLE_COMM_VENC_StopGetStream();
}
//TODO
/******************************************************************************
* function: H.265e + H264e@720P, H.265 Channel resolution adaptable with sensor
******************************************************************************/
HI_S32 CY_ISP_PARAM_SET_HW(void)

{

    HI_S32 ret;

    VI_PIPE ViPipe = 0;



    // Module Bypass

    ISP_MODULE_CTRL_U unModCtrl; 

    ret = HI_MPI_ISP_GetModuleControl(ViPipe, &unModCtrl);

    if ( HI_SUCCESS == ret ) {

        unModCtrl.bitBypassAEStatBE = HI_TRUE;

        unModCtrl.bitBypassAEStatFE = HI_TRUE;

        unModCtrl.bitBypassAFStatBE = HI_TRUE;

        unModCtrl.bitBypassAFStatFE = HI_TRUE;

        unModCtrl.bitBypassAntiFC = HI_TRUE;

        unModCtrl.bitBypassAWBStat = HI_TRUE;

        unModCtrl.bitBypassCA = HI_TRUE;

        unModCtrl.bitBypassCLUT = HI_TRUE;

        unModCtrl.bitBypassColorMatrix = HI_TRUE;

        unModCtrl.bitBypassCrosstalkR = HI_TRUE;

        unModCtrl.bitBypassCsConv = HI_FALSE;

        unModCtrl.bitBypassDE = HI_TRUE;

        unModCtrl.bitBypassDehaze = HI_FALSE;

        unModCtrl.bitBypassDemosaic = HI_TRUE;

        unModCtrl.bitBypassDPC = HI_TRUE;

        unModCtrl.bitBypassDRC = HI_TRUE;

        unModCtrl.bitBypassEdgeMark = HI_TRUE;

        unModCtrl.bitBypassFSWDR = HI_TRUE;

        unModCtrl.bitBypassGamma = HI_FALSE;

        unModCtrl.bitBypassGCAC = HI_TRUE;

        unModCtrl.bitBypassHLC = HI_TRUE;

        unModCtrl.bitBypassISPDGain = HI_TRUE;

        unModCtrl.bitBypassLCAC = HI_TRUE;

        unModCtrl.bitBypassLdci = HI_TRUE;

        unModCtrl.bitBypassMeshShading = HI_TRUE;

        unModCtrl.bitBypassMGStat = HI_TRUE;

        unModCtrl.bitBypassNR = HI_TRUE;

        unModCtrl.bitBypassPreGamma = HI_TRUE;

        unModCtrl.bitBypassRadialCrop = HI_TRUE;

        unModCtrl.bitBypassRadialShading = HI_TRUE;

        unModCtrl.bitBypassRGBIR = HI_TRUE;

        unModCtrl.bitBypassSharpen = HI_TRUE;

        unModCtrl.bitBypassWBGain = HI_TRUE;

        ret = HI_MPI_ISP_SetModuleControl(ViPipe, &unModCtrl);

        if (HI_SUCCESS != ret)

        {

            SAMPLE_PRT("HI_MPI_ISP_SetModuleControl!\n");

            return ret;

        }

    }

    return HI_SUCCESS;

}
HI_S32 SAMPLE_VENC_H265_H264()
{
    HI_S32 i;
    HI_S32 s32Ret;
    SIZE_S          stSize[3];
    PIC_SIZE_E      enSize[3]     = {PIC_1024x1024, PIC_1024x1024,PIC_720P};//PIC_640x512
    HI_S32          s32ChnNum     = 1;
    HI_S32          s32WorkSnsId   = 0;
    VENC_CHN        VencChn[2]    = {0,1};
    HI_U32          u32Profile[2] = {0,0};
    PAYLOAD_TYPE_E  enPayLoad[2]  = {PT_H264, PT_H264};
    VENC_GOP_MODE_E enGopMode;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E     enRcMode;
    HI_BOOL         bRcnRefShareBuf = HI_TRUE;
    PIC_SIZE_E         enPicSize;
    VI_DEV          ViDev        = 0;
    VI_PIPE         ViPipe       = 0;
    VI_CHN          ViChn        = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

   VPSS_GRP_ATTR_S    stVpssGrpAttr;
    VPSS_CHN_ATTR_S    astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    VPSS_GRP        VpssGrp        = 0;
    VPSS_CHN        VpssChn        = VPSS_CHN0;
    HI_BOOL         abChnEnable[4] = {1,0,0,0};

    // HI_U32 u32SupplementConfig = HI_FALSE;

    SAMPLE_VO_CONFIG_S stVoConfig;
    WDR_MODE_E         enWDRMode      = WDR_MODE_NONE;
    DYNAMIC_RANGE_E    enDynamicRange = DYNAMIC_RANGE_SDR8;
    VO_CHN             VoChn          = 0;
    HI_U32             u32BlkSize;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize[0], &stSize[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }


    // u32BlkSize = COMMON_GetPicBufferSize(stSize[0].u32Width, stSize[0].u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    // stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    // stVbConf.astCommPool[0].u32BlkCnt   = 10;
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if(SAMPLE_SNS_TYPE_BUTT == stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType)
    {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n",0);
        return HI_FAILURE;
    }
    // s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    s32Ret = SAMPLE_VENC_VI_Init(&stViConfig, HI_FALSE , HI_FALSE);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Init VI err for %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    // /*config vpss*/

    s32Ret = SAMPLE_VENC_VPSS_Init(VpssGrp,abChnEnable,DYNAMIC_RANGE_SDR8,PIXEL_FORMAT_YVU_SEMIPLANAR_420,stSize,stViConfig.astViInfo[0].stSnsInfo.enSnsType);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Init VPSS err for %#x!\n", s32Ret);
        goto EXIT_VI_STOP;
    }
    
    
        abChnEnable[0] = HI_TRUE;


    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VI Bind VPSS err for %#x!\n", s32Ret);
        goto EXIT_VPSS_STOP;
    }

    s32Ret = CY_ISP_PARAM_SET_HW();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("CY_ISP_PARAM_SET for %#x!\n", s32Ret);
        goto EXIT_VI_STOP;
    }

    // /*config vo*/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    stVoConfig.enDstDynamicRange = enDynamicRange;


    stVoConfig.enVoIntfType = VO_INTF_HDMI;

    //红外分辨率输出
    stVoConfig.enIntfSync   = VO_OUTPUT_1280x1024_60;

    stVoConfig.enPicSize = enSize[0];

    /*start vo*/
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        // goto EXIT4;
    }

    /*vpss bind vo*/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        // goto EXIT5;
    }


   /******************************************
    start stream venc
    ******************************************/

    // enRcMode = SAMPLE_VENC_GetRcMode();
    enRcMode = SAMPLE_RC_CVBR;//SAMPLE_RC_CBR;//在图像内容静止时，节省带宽，有Motion发生时，利用前期节省的带宽来尽可能的提高图像质量，不超过最大码率
    // enGopMode = SAMPLE_VENC_GetGopMode();
    enGopMode = VENC_GOPMODE_NORMALP;//VENC_GOPMODE_DUALP;//该模式下P帧参考就近的两个前向参考帧，能够利用更多参考帧的时域相关性提升编码压缩性能。主要应用在运动且有低延时要求的场景
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(enGopMode,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr for %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

   /***encode h.265 **/
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn[0], enPayLoad[0],enSize[0], enRcMode,u32Profile[0],bRcnRefShareBuf,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_UnBind;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn,VencChn[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_STOP;
    }

    /***encode h.264 **/
    // s32Ret = SAMPLE_COMM_VENC_Start(VencChn[1], enPayLoad[1], enSize[1], enRcMode,u32Profile[1],bRcnRefShareBuf,&stGopAttr);
    // if (HI_SUCCESS != s32Ret)
    // {
    //     SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
    //     goto EXIT_VENC_H264_UnBind;
    // }

    // s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[1],VencChn[1]);
    // if (HI_SUCCESS != s32Ret)
    // {
    //     SAMPLE_PRT("Venc bind Vpss failed for %#x!\n", s32Ret);
    //     goto EXIT_VENC_H264_STOP;
    // }
    //Dump init

    //Dump init end
    /******************************************
     stream save process
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream_rtsp(VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto EXIT_VENC_H264_UnBind;
    }

    pthread_create(&thread_tcp , 0 , Hi_tcpserver , NULL );

    pthread_create(&thread_saveraw , 0 , Hi_SaveRaw , NULL);



    printf("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     exit process
    ******************************************/
    SAMPLE_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
//     SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn[1],VencChn[1]);
// EXIT_VENC_H264_STOP:
//     SAMPLE_COMM_VENC_Stop(VencChn[1]);
EXIT_VENC_H265_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn,VencChn[0]);
EXIT_VENC_H265_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn[0]);
EXIT_VI_VPSS_UNBIND:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe,ViChn,VpssGrp);
EXIT_VPSS_STOP:
    SAMPLE_COMM_VPSS_Stop(VpssGrp,abChnEnable);
EXIT_VI_STOP:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

/******************************************************************************
* function    : main()
* Description : video venc sample
******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char *argv[])
#else
    int main(int argc, char *argv[])
#endif
{
    HI_S32 s32Ret;
    HI_U32 u32Index;

    if (argc < 2 || argc > 2)
    {
        SAMPLE_VENC_Usage(argv[0]);
        return HI_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_VENC_Usage(argv[0]);
        return HI_SUCCESS;
    }

    u32Index = atoi(argv[1]);

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_VENC_HandleSig);
    signal(SIGTERM, SAMPLE_VENC_HandleSig);
#endif

    switch (u32Index)
    {
        case 0:
        // s32Ret = Creat_Uart_Recv();
        // if (HI_SUCCESS != s32Ret)
        // {
        //     SAMPLE_PRT("Start UartRecv failed!\n");
        // }
        // printf("test");
        // getchar();
        // while(1)
        // {
        //     usleep(5000);
        // }
            s32Ret = SAMPLE_VENC_H265_H264();
            // err = pthread_create(&thread_tcp,NULL,tcpserver,NULL);
            // if(err!=0)
            // {
            // printf("thread_tcp_create Failed:%s\n",strerror(errno));
            // }
            // else
            // {
            // printf("thread_tcp_create success\n");
            // }
            break;
        // case 1:
        //     s32Ret = SAMPLE_VENC_LOW_DELAY();
        //     break;
        // case 2:
        //     s32Ret = SAMPLE_VENC_Qpmap();
        //     break;
        // case 3:
        //     s32Ret = SAMPLE_VENC_IntraRefresh();
        //     break;
        // case 4:
        //     s32Ret = SAMPLE_VENC_ROIBG();
        //     break;
        // case 5:
        //     s32Ret = SAMPLE_VENC_DeBreathEffect();
        //     break;
        // case 6:
        //     s32Ret = SAMPLE_VENC_SVC_H264();
        //     break;
        // case 7:
        //     s32Ret = SAMPLE_VENC_MJPEG_JPEG();
        //     break;
        // default:
        //     printf("the index is invaild!\n");
        //     SAMPLE_VENC_Usage(argv[0]);
            return HI_FAILURE;
    }

    // printf("please press twice ENTER to exit this sample\n");
    // getchar();
    // getchar();

    // /******************************************
    //  exit process
    // ******************************************/
    // SAMPLE_COMM_VENC_StopGetStream();

    // if (HI_SUCCESS == s32Ret)
    // { printf("program exit normally!\n"); }
    // else
    // { printf("program exit abnormally!\n"); }

        // PAUSE();


#ifdef __HuaweiLite__
    return s32Ret;
#else
    exit(s32Ret);
#endif
}

// #ifdef __cplusplus
// #if __cplusplus
// }
// #endif
// #endif /* End of #ifdef __cplusplus */
