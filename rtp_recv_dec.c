/******************************************************************************
  A simple program of Hisilicon HI3531 video decode implementation.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-12 Created
******************************************************************************/
//  所有的函数和变量名显示在左边。
//所有的添加到这个项目的文件都显示在右边。现在的这个项目里面的函数都在一个文件里面。

//这是解码段的程序。






#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>  
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "sample_comm.h" 

/***************************************
add by zxb

****************************************/

#include <sys/socket.h>
#include <arpa/inet.h>

#define MCAST_PORT 8888
#define MCAST_ADDR "224.0.0.100"

#define MAX_RTP_LEN 1500
#define BUFF_SIZE   1500

HI_S32 SockRecFd;
struct sockaddr_in Multiaddr;

//static HI_CHAR Rtp_Rec_Buf[MAX_RTP_LEN];

typedef struct 
{
    unsigned char csrc_len:4;        
    unsigned char extension:1;        
    unsigned char padding:1;        
    unsigned char version:2;        
    unsigned char payload:7;   
    unsigned char marker:1;    
    unsigned short seq_no;            
    unsigned  long timestamp;        
    unsigned long ssrc;          
} RTP_FIXED_HEADER;


 

#define SAMPLE_MAX_VDEC_CHN_CNT 8

typedef struct sample_vdec_sendparam
{
    pthread_t Pid;
    HI_BOOL bRun;
    VDEC_CHN VdChn;    
    PAYLOAD_TYPE_E enPayload;
	HI_S32 s32MinBufSize;
    VIDEO_MODE_E enVideoMode;
}SAMPLE_VDEC_SENDPARAM_S;

//HI_S32 gs_VoDevCnt = 4;
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;   //将VIDEO_ENCODING_MODE_PAL的值赋给前面的全局变量gs_enNorm。
HI_U32 gs_u32ViFrmRate = 0;
SAMPLE_VDEC_SENDPARAM_S gs_SendParam[SAMPLE_MAX_VDEC_CHN_CNT];
HI_S32 gs_s32Cnt;


#define SAMPLE_DBG(s32Ret)\
do{\
    printf("s32Ret=%#x,fuc:%s,line:%d\n", s32Ret, __FUNCTION__, __LINE__);\
}while(0)



#define HDMI_OUT



/******************************************************************************
* function : create vdec chn
******************************************************************************/
static HI_S32 SAMPLE_VDEC_CreateVdecChn(HI_S32 s32ChnID, SIZE_S *pstSize, PAYLOAD_TYPE_E enType, VIDEO_MODE_E enVdecMode)
{
    VDEC_CHN_ATTR_S stAttr;
    VDEC_PRTCL_PARAM_S stPrtclParam;
    HI_S32 s32Ret;

    memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));

    stAttr.enType = enType;
    stAttr.u32BufSize = pstSize->u32Height * pstSize->u32Width;//This item should larger than u32Width*u32Height*3/4
    stAttr.u32Priority = 1;//姝ゅ蹇呴』澶т簬0
    stAttr.u32PicWidth = pstSize->u32Width;
    stAttr.u32PicHeight = pstSize->u32Height;
    
    switch (enType)
    {
        case PT_H264:
    	    stAttr.stVdecVideoAttr.u32RefFrameNum = 2;
    	    stAttr.stVdecVideoAttr.enMode = enVdecMode;
    	    stAttr.stVdecVideoAttr.s32SupportBFrame = 0;
            break;
        case PT_JPEG:
            stAttr.stVdecJpegAttr.enMode = enVdecMode;
            break;
        case PT_MJPEG:
            stAttr.stVdecJpegAttr.enMode = enVdecMode;
            break;
        default:
            SAMPLE_PRT("err type \n");
            return HI_FAILURE;
    }

    s32Ret = HI_MPI_VDEC_CreateChn(s32ChnID, &stAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_CreateChn failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VDEC_GetPrtclParam(s32ChnID, &stPrtclParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_GetPrtclParam failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    stPrtclParam.s32MaxSpsNum = 21;
    stPrtclParam.s32MaxPpsNum = 22;
    stPrtclParam.s32MaxSliceNum = 100;
    s32Ret = HI_MPI_VDEC_SetPrtclParam(s32ChnID, &stPrtclParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_SetPrtclParam failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_VDEC_StartRecvStream(s32ChnID);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VDEC_StartRecvStream failed errno 0x%x \n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}


static char* SAMPLE_AUDIO_Pt2Str(PAYLOAD_TYPE_E enType)
{
    if (PT_G711A == enType)  return "g711a";
    else if (PT_G711U == enType)  return "g711u";
    else if (PT_ADPCMA == enType)  return "adpcm";
    else if (PT_G726 == enType)  return "g726";
    else if (PT_LPCM == enType)  return "pcm";
    else return "data";
}



static FILE *SAMPLE_AUDIO_OpenAdecFile(ADEC_CHN AdChn, PAYLOAD_TYPE_E enType)
{
    FILE *pfd;
    HI_CHAR aszFileName[128];
    
    /* create file for save stream*/        
    sprintf(aszFileName, "audio_chn%d.%s", AdChn, SAMPLE_AUDIO_Pt2Str(enType));
    pfd = fopen(aszFileName, "rb");
    if (NULL == pfd)
    {
        printf("%s: open file %s failed\n", __FUNCTION__, aszFileName);
        return NULL;
    }
    printf("open stream file:\"%s\" for adec ok\n", aszFileName);
    return pfd;
}


#define SAMPLE_AUDIO_AO_DEV 0    /*修改为1*/

static HI_BOOL gs_bMicIn = HI_FALSE;
static PAYLOAD_TYPE_E gs_enPayloadType = PT_ADPCMA;   /*音频格式*/
static AUDIO_RESAMPLE_ATTR_S *gs_pstAoReSmpAttr = NULL;
#define SAMPLE_AUDIO_PTNUMPERFRM   320
static AUDIO_RESAMPLE_ATTR_S *gs_pstAiReSmpAttr = NULL;




FILE        *pfd = NULL;


HI_S32 AUDIO_AdecAo(AIO_ATTR_S *pstAioAttr)
{
        HI_S32      s32Ret;
        AUDIO_DEV   AoDev ;
        AO_CHN      AoChn = 0;
        ADEC_CHN    AdChn = 0;
      

#ifdef HDMI_OUT
        AoDev = SAMPLE_AUDIO_HDMI_AO_DEV;
#else
        AoDev = SAMPLE_AUDIO_AO_DEV;
#endif

        if (NULL == pstAioAttr)
        {
            printf("[Func]:%s [Line]:%d [Info]:%s\n", __FUNCTION__, __LINE__, "NULL pointer");
            return HI_FAILURE;
        }
/*放到函数前面去了*/
      //  s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(pstAioAttr, gs_bMicIn);
      //  if (HI_SUCCESS != s32Ret)
        //{
         //   SAMPLE_DBG(s32Ret);
           // return HI_FAILURE;
       // }
        
        s32Ret = SAMPLE_COMM_AUDIO_StartAdec(AdChn, gs_enPayloadType);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_DBG(s32Ret);
            return HI_FAILURE;
        }

        s32Ret = SAMPLE_COMM_AUDIO_StartAo(AoDev, AoChn, pstAioAttr, gs_pstAoReSmpAttr);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_DBG(s32Ret);
            return HI_FAILURE;
        }

        s32Ret = SAMPLE_COMM_AUDIO_AoBindAdec(AoDev, AoChn, AdChn);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_DBG(s32Ret);
            return HI_FAILURE;
        }

        //return HI_SUCCESS;


        pfd = SAMPLE_AUDIO_OpenAdecFile(AdChn, gs_enPayloadType);
        if (!pfd)
        {
            SAMPLE_DBG(HI_FAILURE);
            return HI_FAILURE;
        }
#if 0

        s32Ret = SAMPLE_COMM_AUDIO_CreatTrdFileAdec(AdChn, pfd);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_DBG(s32Ret);
            return HI_FAILURE;
        }
        
        printf("bind adec:%d to ao(%d,%d) ok \n", AdChn, AoDev, AoChn);

        printf("\nplease press twice ENTER to exit this sample\n");
        getchar();
        getchar();

        //SAMPLE_COMM_AUDIO_DestoryTrdFileAdec(AdChn);
       // SAMPLE_COMM_AUDIO_StopAo(AoDev, AoChn, gs_bAioReSample);
       // SAMPLE_COMM_AUDIO_StopAdec(AdChn);
       // SAMPLE_COMM_AUDIO_AoUnbindAdec(AoDev, AoChn, AdChn);
#endif
        return HI_SUCCESS;
}



static FILE * SAMPLE_AUDIO_OpenAencFile(AENC_CHN AeChn, PAYLOAD_TYPE_E enType)
{
    FILE *pfd;
    HI_CHAR aszFileName[128];
    
    /* create file for save stream*/
    sprintf(aszFileName, "audio_chn%d.%s", AeChn, SAMPLE_AUDIO_Pt2Str(enType));
    pfd = fopen(aszFileName, "w+");
    if (NULL == pfd)
    {
        printf("%s: open file %s failed\n", __FUNCTION__, aszFileName);
        return NULL;
    }
    printf("open stream file:\"%s\" for aenc ok\n", aszFileName);
    return pfd;
}


/*初始化音频设备输出*/ /*可以设置流解码和帧解码方式*/
HI_S32 Init_AUDIO_AdecAo()
{
        AIO_ATTR_S stAioAttr;
        AIO_ATTR_S stHdmiAoAttr;

         AUDIO_RESAMPLE_ATTR_S stAoReSampleAttr;

        /* init stAio. all of cases will use it */
        stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_8000;
        stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
        stAioAttr.enWorkmode = AIO_MODE_I2S_SLAVE;
        stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
        stAioAttr.u32EXFlag = 1;
        stAioAttr.u32FrmNum = 30;
        stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM;
        stAioAttr.u32ChnCnt = 2;
        stAioAttr.u32ClkSel = 0;

#ifdef HDMI_OUT
/*hdmi 输出*/
        gs_pstAiReSmpAttr = NULL;
                
        /* ao 8k -> 48k */
        stAoReSampleAttr.u32InPointNum = SAMPLE_AUDIO_PTNUMPERFRM;
        stAoReSampleAttr.enInSampleRate = AUDIO_SAMPLE_RATE_8000;
        stAoReSampleAttr.enReSampleType = AUDIO_RESAMPLE_1X6;
        gs_pstAoReSmpAttr = &stAoReSampleAttr;
        
        memcpy(&stHdmiAoAttr, &stAioAttr, sizeof(AIO_ATTR_S));
        stHdmiAoAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
        stHdmiAoAttr.enSamplerate = AUDIO_SAMPLE_RATE_48000;
        stHdmiAoAttr.u32PtNumPerFrm = stAioAttr.u32PtNumPerFrm * 6;
        stHdmiAoAttr.enWorkmode = AIO_MODE_I2S_MASTER;
        stHdmiAoAttr.u32ChnCnt = 2;

        SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
    
        AUDIO_AdecAo(&stHdmiAoAttr);

#if 0
		FILE *pfd;
		pfd= SAMPLE_AUDIO_OpenAencFile(0, gs_enPayloadType); 
		SAMPLE_COMM_AUDIO_CreatTrdAencAdec(0, 0, pfd);
		while(1) sleep(5);
#endif		
/***************************************/
#else
        SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
        AUDIO_AdecAo(&stAioAttr);
#endif

        

        return HI_SUCCESS;
}


void SAMPLE_VDEC_WaitDestroyVdecChn(HI_S32 s32ChnID, VIDEO_MODE_E enVdecMode)
{
    HI_S32 s32Ret;
    VDEC_CHN_STAT_S stStat;

    memset(&stStat, 0, sizeof(VDEC_CHN_STAT_S));

    s32Ret = HI_MPI_VDEC_StopRecvStream(s32ChnID);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VDEC_StopRecvStream failed errno 0x%x \n", s32Ret);
        return;
    }

    /*** wait destory ONLY used at frame mode! ***/
    if (VIDEO_MODE_FRAME == enVdecMode)
    {
        while (1)
        {
            //printf("LeftPics:%d, LeftStreamFrames:%d\n", stStat.u32LeftPics,stStat.u32LeftStreamFrames);
            usleep(40000);
            s32Ret = HI_MPI_VDEC_Query(s32ChnID, &stStat);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VDEC_Query failed errno 0x%x \n", s32Ret);
                return;
            }
            if ((stStat.u32LeftPics == 0) && (stStat.u32LeftStreamFrames == 0))
            {
                printf("had no stream and pic left\n");
                break;
            }
        }
    }
    s32Ret = HI_MPI_VDEC_DestroyChn(s32ChnID);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VDEC_DestroyChn failed errno 0x%x \n", s32Ret);
        return;
    }
}


int Mpp_Sys_Init(HI_S32 s32Cnt)
{
    VB_CONF_S stVbConf; 
    HI_U32 u32WndNum, u32BlkSize; 
    HI_S32 s32Ret;   
    
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
               PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

	
    stVbConf.u32MaxPoolCnt = 128;

    /* hist buf*/
    stVbConf.astCommPool[0].u32BlkSize = (196*4);
    stVbConf.astCommPool[0].u32BlkCnt = s32Cnt * 6;
    memset(stVbConf.astCommPool[0].acMmzName,0,
        sizeof(stVbConf.astCommPool[0].acMmzName));
	
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("mpp init failed!\n");
        return HI_FAILURE;
    }


}


/******************************************************************************
* function : vdec process
*            vo is sd : vdec -> vo
*            vo is hd : vdec -> vpss -> vo
******************************************************************************/
HI_S32 Init_MultiSock_VDEC_Process(PIC_SIZE_E enPicSize, PAYLOAD_TYPE_E enType, HI_S32 s32Cnt, VO_DEV VoDev)//为解码做一个硬件初始化
{
    VDEC_CHN VdChn;// 定义一个解码通道的变量
    HI_S32 s32Ret;   //定义一个返回的变量，看函数执行是正确还是错误。
    SIZE_S stSize;   //定义一个大小的变量
    VB_CONF_S stVbConf;   //是一个结构体变量，结构体包含其他的成员，或者说包含其他的变量。
    HI_S32 i;   //定义一个整数变量。
    VPSS_GRP VpssGrp;  
    VIDEO_MODE_E enVdecMode;   //定义一个解码模式变量
    HI_CHAR ch;  //定义一个通道变量。

    VO_CHN VoChn;  //定义一个输出通道变量
    VO_PUB_ATTR_S stVoPubAttr;     //定义一个结构体
    SAMPLE_VO_MODE_E enVoMode;     //定义一个输出模式的变量
    HI_U32 u32WndNum, u32BlkSize;   //定义变量，后面是定义一个块的大小

    HI_BOOL bVoHd; // through Vpss or not. if vo is SD, needn't through vpss     //定义一个布尔变量

#ifdef HDMI_OUT
    VO_INTF_TYPE_E vo_interface_type = VO_INTF_HDMI; /*输出接口改为HDMI*/
#else
     VO_INTF_TYPE_E vo_interface_type = VO_INTF_VGA;
#endif
 
    /******************************************
     step 1: init varaible.
    ******************************************/
    gs_u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL == gs_enNorm)?25:30;   //获取视频的制式，是25帧还是30帧。通过全局变量gs_enNorm，了解到是PAL还是NTSC，把帧数25还是30赋予给变量。如果括号里面的是真，那么赋值给前面的变量就是25，如果是假，那么就赋给后面的30 。
    
    if (s32Cnt > SAMPLE_MAX_VDEC_CHN_CNT || s32Cnt <= 0)   //判断这个变量值是否超过 SAMPLE_MAX_VDEC_CHN_CNT，判断输入的解码通道值是否超过最大值，或者小于0，也就是没有，那么返回错误。s32Cnt是上面的Init_MultiSock_VDEC_Process函数中的一个参数。
    {
        SAMPLE_PRT("Vdec count %d err, should be in [%d,%d]. \n", s32Cnt, 1, SAMPLE_MAX_VDEC_CHN_CNT);  //如果上一句的判断是小于0的或者大于解码通道最大值，那么就显示错误信息，并且跳出本函数
        
        return HI_FAILURE;  //跳出本函数
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enPicSize, &stSize);  //这个函数的功能是得到特定编码的图片的大小比例。不同分辨率的长跟宽是不一样的
    if (HI_SUCCESS !=s32Ret)  //如果上述函数得到一个返回值赋给s32ret  ，是假，就执行下面的显示错误信息，并返回一个错误值给调用这个函数的那个更大的函数。
    {
        SAMPLE_PRT("get picture size failed!\n");   //显示错误报告。
        return HI_FAILURE;
    }
    if (704 == stSize.u32Width)  // 704是视频格式为D1的宽度值，从头文件里面可以查到。头文件芯片厂家的SDK里面提供的。
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)  //352是等于CIF的格式。
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)  //176是QCIF，相当很分辨率很低的
    {
        stSize.u32Width = 180;
    }
    
    // through Vpss or not. if vo is SD, needn't through vpss
    if (SAMPLE_VO_DEV_DHD0 != VoDev )   //根据变量VoDev ,Vodev是我们在MAIN数里面手工赋值的。判断输出设备是高清的还是标清的，
    {
        bVoHd = HI_FALSE; //如果判断后，Vodev不等于SAMPLE_VO_DEV_DHD0 ，也即是高清格式，那么赋值。
    }
    else
    {
        bVoHd = HI_TRUE;
    }

#if 0 /*mpp init 见函数Mpp_Sys_Init*/
    /******************************************
     step 2: mpp system init.
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
               PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

	
    stVbConf.u32MaxPoolCnt = 128;

    /* hist buf*/
    stVbConf.astCommPool[0].u32BlkSize = (196*4);
    stVbConf.astCommPool[0].u32BlkCnt = s32Cnt * 6;
    memset(stVbConf.astCommPool[0].acMmzName,0,
        sizeof(stVbConf.astCommPool[0].acMmzName));
	
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("mpp init failed!\n");
        return HI_FAILURE;
    }
 #endif
    /******************************************
     step 3: start vpss, if ov is hd.
    ******************************************/  
#if 1
    if (HI_TRUE == bVoHd)
    {
        s32Ret = SAMPLE_COMM_VPSS_Start(s32Cnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
        if (HI_SUCCESS !=s32Ret)
        {
            SAMPLE_PRT("vpss start failed!\n");
            goto END_0;
        }
    }

#endif
    /******************************************
     step 4: start vo
    ******************************************/
    u32WndNum = 1;
    enVoMode = VO_MODE_1MUX;

   
      
     //stVoPubAttr.enIntfSync = VO_OUTPUT_720P50;
	 stVoPubAttr.enIntfSync= VO_OUTPUT_1080P25; /*影响之前全屏的问题*/
    
    stVoPubAttr.enIntfType = vo_interface_type;//VO_INTF_VGA;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;

 #if 0 /*音频解码初始化了*/
    s32Ret = SAMPLE_COMM_VO_StartDevLayer(VoDev, &stVoPubAttr, gs_u32ViFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartDevLayer failed!\n");
        goto END_1;
    }
#endif  
    s32Ret = SAMPLE_COMM_VO_StartChn(VoDev, &stVoPubAttr, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_2;
    }

#if 0 /*音频解码初始化有做HDMI输出初始化*/
if (HI_TRUE == bVoHd)
    {
        /* if it's displayed on HDMI, we should start HDMI */
        if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
        {
            if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
            {
                SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
                goto END_1;
            }
        }
    }
#endif
 
#if 1
    for(i=0;i<u32WndNum;i++)
    {
        VoChn = i;

        if (HI_TRUE == bVoHd)
        {
            VpssGrp = i;
            s32Ret = SAMPLE_COMM_VO_BindVpss(VoDev,VoChn,VpssGrp,VPSS_PRE0_CHN);
            if (HI_SUCCESS != s32Ret)
            {
                SAMPLE_PRT("SAMPLE_COMM_VO_BindVpss failed!\n");
                goto END_2;
            }
        }
    }
#endif
    /******************************************
     step 5: start vdec & bind it to vpss or vo
    ******************************************/ 
     
    enVdecMode = VIDEO_MODE_FRAME; //VIDEO_MODE_STREAM
   
    for (i=0; i<s32Cnt; i++)
    {
        /***** create vdec chn *****/
        VdChn = i;        
        s32Ret = SAMPLE_VDEC_CreateVdecChn(VdChn, &stSize, enType, enVdecMode);
       
#if 1     
        /***** bind vdec to vpss *****/
        if (HI_TRUE == bVoHd)
        {
            VpssGrp = i;
            s32Ret = SAMLE_COMM_VDEC_BindVpss(VdChn, VpssGrp);
            if (HI_SUCCESS !=s32Ret)
            {
                SAMPLE_PRT("vdec(vdch=%d) bind vpss(vpssg=%d) failed!\n", VdChn, VpssGrp);
                goto END_3;
            }
        }
        else
    	{
            VoChn =  i;
            s32Ret = SAMLE_COMM_VDEC_BindVo(VdChn, VoDev, VoChn);
            if (HI_SUCCESS !=s32Ret)
            {
                SAMPLE_PRT("vdec(vdch=%d) bind vpss(vpssg=%d) failed!\n", VdChn, VpssGrp);
                goto END_3;
            }
        }
#endif
    }

	return HI_SUCCESS;

 
    /******************************************
     step : Unbind vdec to vpss & destroy vdec-chn
    ******************************************/
END_3:
    for (i=0; i<s32Cnt; i++)
    {
        VdChn = i;
        SAMPLE_VDEC_WaitDestroyVdecChn(VdChn, enVdecMode);
        if (HI_TRUE == bVoHd)
        {
            VpssGrp = i;
            SAMLE_COMM_VDEC_UnBindVpss(VdChn, VpssGrp);
        }
        else
        {
            VoChn = i;
            SAMLE_COMM_VDEC_UnBindVo(VdChn, VoDev, VoChn);
        }
    }
    /******************************************
     step : stop vo
    ******************************************/
END_2:
    SAMPLE_COMM_VO_StopChn(VoDev, enVoMode);
    for(i=0;i<u32WndNum;i++)
    {
        VoChn = i;
        if (HI_TRUE == bVoHd)
        {
            VpssGrp = i;
            SAMPLE_COMM_VO_UnBindVpss(VoDev,VoChn,VpssGrp,VPSS_PRE0_CHN);
        }
    }
    SAMPLE_COMM_VO_StopDevLayer(VoDev);
END_1:
    if (HI_TRUE == bVoHd)
    {
        if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
        {
            SAMPLE_COMM_VO_HdmiStop();
        }
        SAMPLE_COMM_VPSS_Stop(s32Cnt, VPSS_MAX_CHN_NUM);
    }
    /******************************************
     step : exit mpp system
    ******************************************/
END_0:
    SAMPLE_COMM_SYS_Exit();


    return HI_SUCCESS;
}

#if 0
struct ip_mreq mreq;

HI_S32 IntiRtpRecSock()
{
	HI_S32 s;
	HI_S32 err = -1;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
	{
		perror("socket()");
		return -1;
	}

	SockRecFd = s;

	printf("sock fd = %d \n", SockRecFd);
	
	memset(&Multiaddr, 0, sizeof(Multiaddr));
	Multiaddr.sin_family = AF_INET;
	Multiaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	Multiaddr.sin_port = htons(MULTIPORT);


	err = bind(s,(struct sockaddr*)&Multiaddr, sizeof(Multiaddr)) ;
	if(err < 0)
	{
		perror("bind()");
		return -2;
	}

	int loop = 1;
	err = setsockopt(s,IPPROTO_IP, IP_MULTICAST_LOOP,&loop, sizeof(loop));
	if(err < 0)
	{
		perror("setsockopt():IP_MULTICAST_LOOP");
		return -3;
	}

	//struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(MULTIADDR); 
	mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

	err = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq));
	if (err < 0)
	{
		perror("setsockopt():IP_ADD_MEMBERSHIP");
		return -4;
	}

	return HI_SUCCESS;
}
#define RECBUFFLEN   1500

char recbuff[1500];

void MultiRecPacked()
{
	socklen_t  addr_len;
	int n;
	int times = 0;
	while(1)
	{	
		times++;
		addr_len = sizeof(Multiaddr);
		memset(recbuff, 0, RECBUFFLEN);

		n = recvfrom(SockRecFd, recbuff, RECBUFFLEN, 0,(struct sockaddr*)&Multiaddr, &addr_len);
		if( n== -1)
		{
		perror("recvfrom()");
		}

		printf("Recv %dst message from server:%s\n", times, recbuff);
	}
	setsockopt(SockRecFd, IPPROTO_IP, IP_DROP_MEMBERSHIP,&mreq, sizeof(mreq));

}

#endif

HI_U64  u64PTS = 0;  

void MultiPacket_VDEC_SendStream(unsigned char *buff, HI_U32 u32Len, unsigned int PTS)
{
	VDEC_STREAM_S stStream;

	//u64PTS = u64PTS + (PTS*40000/3600);
	u64PTS = u64PTS + PTS;
	stStream.u64PTS  = u64PTS;
    stStream.pu8Addr = buff;
    stStream.u32Len  = u32Len;
	//u64PTS += 40000;
	HI_MPI_VDEC_SendStream(0, &stStream, HI_IO_BLOCK);
	
}

typedef enum rtp_pac_marker
{
	rtp_pac_end,
	rtp_pac_inter,   	
	rtp_pac_error,
	
}rtp_marker_type;


/*解析RTP 帧头*/
int Parse_RtpHeader(unsigned char *buff, unsigned int len, unsigned int *pre_pts, int *payloadoffset, int *payload_type)
{
	if(!buff || (12 > len))
		return rtp_pac_error;
	
	int i =0;
	RTP_FIXED_HEADER  *rtp_hdr;
	int payloadlen = 12;

	rtp_hdr =(RTP_FIXED_HEADER*)&buff[0]; 

#if 0
	for(i = 0; i < 12; i++)
	{
		//printf(" %d -", buff[i]);
	}
	//printf("\n");


	printf("version = %d , payload = %d ,marker = %d, uts = %ld\n", rtp_hdr->version, rtp_hdr->payload, rtp_hdr->marker, ntohl(rtp_hdr->timestamp));
#endif

	//printf("#############seq = %d\n", ntohs(rtp_hdr->seq_no));
	//96是h264 视频格式的类型，音频的负载类型值也是根据音频编码区分的
	*payload_type = rtp_hdr->payload;

	if((2 != rtp_hdr->version)  && ((96 == *payload_type) || (8 == *payload_type)) )
	{
		//rtp 版本错误或者RTP 负载类型不是96(H264)  ,  ；
		//是否还需要在判断下ssrc ，信息源
		//还需要判断扩展数据长度以及csrc len ， 
		printf("rtp para wrong\n");
		return rtp_pac_error;
	}

	
#if 1
		payloadlen += 4*(rtp_hdr->csrc_len);
	
		if(payloadlen >= len)
			return rtp_pac_error;
	
	
		if(rtp_hdr->extension)
		{
	
			if((payloadlen + 4) >= len)
				return rtp_pac_error;
	
			
			unsigned char *exten_data = &buff[payloadlen];
	
			int exten_data_len = 4 * (exten_data[2]<<8 | exten_data[3]);
	
			payloadlen += 4 + exten_data_len;
			
		}
#endif


	*pre_pts = ntohl(rtp_hdr->timestamp);
	*payloadoffset = payloadlen;
	//printf("marker = %d   seq = %d  %x %d\n", rtp_hdr->marker, ntohs(rtp_hdr->seq_no), buff[1], len);
	if(rtp_hdr->marker == 1)
	{
		//接收一帧视频数据完毕
		//printf("rtp_pac_end\n");
		return rtp_pac_end;
	}
	else
	{
		//还需要接收到一帧数据中的最后一个rtp 包
		
		return rtp_pac_inter;
	}
		

}


int IntiRtpRecSock(struct sockaddr_in *addr)
{
	int s;
	struct sockaddr_in local_addr;
	int err = -1;
	rtp_marker_type rtp_marker;
	int payloadoffset;
        int payload_type;

	//建立组播网络socket
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
	{
		perror("socket()");
		return -1;
	}

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(MCAST_PORT);


	err = bind(s,(struct sockaddr*)&local_addr, sizeof(local_addr)) ;
	if(err < 0)
	{
		perror("bind()");
		return -2;
	}


	int loop = 1;
	err = setsockopt(s,IPPROTO_IP, IP_MULTICAST_LOOP,&loop, sizeof(loop));
	if(err < 0)
	{
		perror("setsockopt():IP_MULTICAST_LOOP");
		return -3;
	}

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR); 
	mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

    //加入组播组
	err = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq));
	
	if (err < 0)
	{
		perror("setsockopt():IP_ADD_MEMBERSHIP");
		return -4;
	}
	memcpy(addr, &local_addr, sizeof(local_addr));
    return s;

	int times = 0;
	socklen_t addr_len = 0;
	unsigned char buff[BUFF_SIZE];
	unsigned char framebuff[311040];
	unsigned int framelen = 0;
	unsigned int preUTS = 0, nowUTS = 0;
	int n = 0;

	/////////////////////////////////
	int i =0;
	HI_CHAR aszFileName[64];
	FILE *pFile;
	char szFilePostfix[10];

	strcpy(szFilePostfix, ".h264");
	sprintf(aszFileName, "/nfsroot/stream_chn%d%s", i, szFilePostfix);

	pFile = fopen(aszFileName, "wb");
    if (!pFile)
    {
        SAMPLE_PRT("open file[%s] failed!\n", 
               aszFileName);
        return -1;
    }


	/////////////////////////////////
	addr_len = sizeof(local_addr);

	//循环接收H264 RTP视频数据组播报文
	printf("start mulit h264 .\n");
	while(1)
	{	
		times++;
		
		memset(buff, 0, BUFF_SIZE);

		n = recvfrom(s, buff, BUFF_SIZE, 0,(struct sockaddr*)&local_addr,&addr_len);
		if( n== -1)
		{
			perror("recvfrom()");
			continue;
		}
		//printf("Recv  index  message : len = %d  \n", n);
		rtp_marker = Parse_RtpHeader(buff, n, &nowUTS, &payloadoffset, &payload_type);

		if(rtp_marker == rtp_pac_end)
		{
			memcpy(framebuff + framelen, buff + payloadoffset, n - payloadoffset);
			framelen = framelen + n - payloadoffset;
		}
		else if(rtp_marker == rtp_pac_inter)
		{
			memcpy(framebuff + framelen, buff + payloadoffset, n - payloadoffset);
			framelen = framelen + n - payloadoffset;
			continue;
		}
		else if(rtp_marker == rtp_pac_error)
		{
			continue;
		}
		
#if 0	
		memcpy(&nowUTS, &buff[2], sizeof(nowUTS));
		printf("Recv %d index %d message : len = %d , pts %d \n", buff[0], buff[1], n, nowUTS);
		//if(nowUTS == preUTS)
		{
			//preUTS = nowUTS;

			if(buff[0] < buff[1])
			{
				memcpy(framebuff + (1480*(buff[0] - 1)), buff+6, n-6);
				framelen = framelen + 1480;
				continue;
			}
			else if(buff[0] == buff[1])
			{
				memcpy(framebuff + (1480*(buff[1] - 1)) , buff+6, n-6);
				framelen = 1480 * (buff[1] - 1) + n -6;
			}
		}
		//else
		{
			;
		}
#endif	
		//将接收到的一帧视频数据进行解码
		MultiPacket_VDEC_SendStream(framebuff, framelen, (nowUTS - preUTS));

		framelen = 0;

		preUTS = nowUTS;

		
#if 0
		fwrite(buff, n, 1, pFile);

        fflush(pFile);

		printf("Recv %dst message : len = %d \n", times, n);
#endif	

	}

	err = setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));

	close(s);

	return 1;

}

int InitRtpUdpSock(int port)
{
	int sockid, len;
	struct sockaddr_in local_addr;

	char buff[1500];
	struct sockaddr_in servsockaddr;

	socklen_t addr_len = sizeof(servsockaddr); 

	sockid = socket(AF_INET,SOCK_DGRAM, 0);

	
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(port);
	bind(sockid,(struct sockaddr*)&local_addr, sizeof(local_addr)) ;
	printf("recv \n");

	//while(1)
	{
		len = recvfrom(sockid, buff, 1500, 0, (struct sockaddr *)&servsockaddr, &addr_len);
		printf("len = %d \n", len);
	}
	return sockid;
}

int InitRtpTcpSock(int port, char *addr, struct sockaddr_in *serversockaddr)
{
	int sockid;
	struct sockaddr_in servaddr;
	struct sockaddr_in local_addr;
	
	sockid = socket(AF_INET,SOCK_STREAM, 0);

	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(8888);
	bind(sockid,(struct sockaddr*)&local_addr, sizeof(local_addr)) ;

	

	 memset(&servaddr, 0, sizeof(servaddr));
     servaddr.sin_family = AF_INET;
     servaddr.sin_port = htons(port);  ///服务器端口
     servaddr.sin_addr.s_addr = inet_addr(addr);  ///服务器ip
	//int flag = 0;
	// ioctl(sockid,FIONBIO,&flag);
	 connect(sockid, (struct sockaddr *)&servaddr, sizeof(servaddr));
	char bu[256];
	//recvfrom(sockid, bu, sizeof(bu), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	recv(sockid, bu, sizeof(bu), 0);
	printf("############## %s \n", bu);

	memcpy(serversockaddr, &servaddr, sizeof(servaddr));
	return sockid;
}

/****************************************************************************
* function: main
****************************************************************************/
    HI_U64  u64Audio_PTS = 0;  



int main(int argc, char* argv[] )
{
	int sockid;  //SOCKET  ID 变量      
	unsigned char buff[1500];   //接收端，MTU以太网口通讯最大报文一般是1500字节，
	int len;   
	int payloadoffset;
	unsigned int preUTS = 0,nowUTS = 0;
	rtp_marker_type rtp_marker;
	unsigned char framebuff[311040];
	unsigned int framelen = 0;

        unsigned char audioframebuff[10*1024];
        unsigned int audioframelen = 0;
    
	VDEC_STREAM_S stStream;

        int paypload_type;
        unsigned int audio_preUTS = 0, audio_nowUTS = 0;
        AUDIO_STREAM_S stAudioStream;    

        //stAudioStream.u32Seq = 1;

	char *cmd = "ifconfig eth0 192.168.1.20";
	system(cmd);     //上面两个语句，就是把IP地址指定为192.168.1.20  。但通常的话，指定IP地址会在LINUX脚本里面指定。
	
	//   char *cmd = "route add -net 224.0.0.0 netmask 224.0.0.0 eth0";  //添加一个路由
	
	// system(cmd); //执行如上的命令，上一行是一个系统命令

	gs_s32Cnt = 1;//这是一个全局变量，设置输出时候是一个屏幕。

        Mpp_Sys_Init(gs_s32Cnt);

        
		Init_AUDIO_AdecAo();

		sleep(1);

	Init_MultiSock_VDEC_Process(PIC_D1, PT_H264, gs_s32Cnt, SAMPLE_VO_DEV_DHD0);//为解码做准备的函数，初始化硬件。括号里面的是函数的参数，这个函数在执行的时候 带着里面的变量执行。括号里面的变量已经有赋值里。

	
        
     struct sockaddr_in servsockaddr;

	//sockid = IntiRtpRecSock(&servsockaddr);//初始化组播SOCKET,接收并解析组播的RTP包报文。后续在函数里面在写程序。

	//struct sockaddr_in servsockaddr;

	//sockid = InitRtpTcpSock(8888, "192.168.1.10", &servsockaddr); //建立TCP socket

	sockid = InitRtpUdpSock(8888);      //建立UDPsocket,  现在建立的是UPD的socket ,如果是TCP，则用别的函数。

	// 里面调用了LINUX 系统API 函数 socket。 SOCKET号，后面程序就用SOCKET号来通讯，就不是用IP地址来通讯的。
	
	socklen_t addr_len = sizeof(servsockaddr);   // servsockaddr这是一个系统结构体，里面有IPV4和IPV6协议类型，还有端口和地址等

//sizeof是C库里面的运算符，是算出结构体的长度。

	printf("sockid %d\n",sockid);
	int u32ReadLen, u32Len = 168;
	
	while(1)
	{	
		
		#if 0
        u32ReadLen = fread(framebuff, 1, u32Len, pfd);
		stAudioStream.pStream = framebuff;
		stAudioStream.u32Len = u32ReadLen;
		printf("len = %d\n", u32ReadLen);
         HI_MPI_ADEC_SendStream(0, &stAudioStream, HI_TRUE);
		 continue;
		#endif



		

		memset(buff, 0, 1500);   //memset是系统函数，对内存进行设置。这在别人的程序里面也是这个名称。buff是我们上面定义的变量。0是把内存里面的都置为0,1500是BUFF的大小。
		len = recvfrom(sockid, buff, 1500, 0, (struct sockaddr *)&servsockaddr, &addr_len);  //recvfrom从SOCKID的SOCKET获取数据报文。获取的报文放在buff里面。接收到的报文长度告诉len。
		//if(len > 0)
		//	printf("len = %d \n", len);

	    //continue;
		//len = recv(sockid, buff, 1500, 0);
		
		//continue;
		//printf("recv success \n");
		rtp_marker = Parse_RtpHeader(buff, len, &nowUTS, &payloadoffset, &paypload_type);  //buff接收到的报文，由RTP格式进行解码。并把是否是一帧数据的报文，返回给RTPMAKER ,目的是判断是否一帧最后报文，如果是，就用输出函数来解码播放。
		if(rtp_marker == rtp_pac_end) //一帧接收完毕将之前接收的数据组装成一帧的视频数据
		{
		       if(paypload_type == 96)
			{
			        memcpy(framebuff + framelen, buff + payloadoffset, len - payloadoffset);//如果接收到是帧的结尾，就进行内存拷贝，把BUFF拷贝到帧缓存FRAMEBUFF里面，输出函数会来FRAMEBUFF里面取数据输出。
			        framelen = framelen + len - payloadoffset;  //可以得到一帧的长度，后面输出函数需要用到。
                    }
                    else
                    {
                            memcpy(audioframebuff + audioframelen, buff + payloadoffset, len - payloadoffset);
			        audioframelen = audioframelen + len - payloadoffset; 
                    }
		}
		else if(rtp_marker == rtp_pac_inter)//一帧未接收完毕，则再continue 循环接收下一个RTP报文
		{
		       if(paypload_type == 96)
                    {      
			        memcpy(framebuff + framelen, buff + payloadoffset, len - payloadoffset);
			        framelen = framelen + len - payloadoffset;
                    }
                    else 
                     {
                            memcpy(audioframebuff + audioframelen, buff + payloadoffset, len - payloadoffset);
			        audioframelen = audioframelen + len - payloadoffset; 
                    }
			
			continue;  //continue是和while对应的。就是说如果还不是帧的结尾，就继续接收下一个报文。
		}
		else
		{
			continue;
		}
		//printf("#########################preuts\n" );
        	if(paypload_type == 96)
			{
                    //printf("time %d len %d\n\n", nowUTS - preUTS, framelen);
                    u64PTS = u64PTS + (nowUTS - preUTS); //时间戳递增
                    stStream.u64PTS  = u64PTS;
                    stStream.pu8Addr = framebuff; //一帧视频的地址
                    stStream.u32Len  = framelen; //一帧视频的长度

                    HI_MPI_VDEC_SendStream(0, &stStream, HI_IO_BLOCK);   //HI_MPI_VDEC_SendStream就是SDK里面的内容。就是由芯片厂家提供的。
                    //ststream是一个结构体，这个结构体包含了三个变量，stStream.u64PTS是SDK里面指定的时间戳变量。stStream.pu8Addr 是SDK里面指定的帧地址的变量。stStream.u32Len是SDK里指定的帧的长度。
                    framelen = 0;  //要开始接收下一帧数据，所以要把帧的长度清零。

                    preUTS = nowUTS; //把现在的时间戳时间变成上一帧时间了。
              }
              else
              {
                       // printf("audio time %d len %d\n", nowUTS - audio_preUTS, framelen);
                        u64Audio_PTS = u64Audio_PTS +  (audio_nowUTS - audio_preUTS); //时间戳递增
                        stAudioStream.u64TimeStamp = u64Audio_PTS;
                        stAudioStream.pStream = audioframebuff;
                        stAudioStream.u32Len = audioframelen;

						//printf("%02x %02x %02x %02x \n\n", stAudioStream.pStream[0],stAudioStream.pStream[1],stAudioStream.pStream[2],stAudioStream.pStream[3]);

                        HI_MPI_ADEC_SendStream(0, &stAudioStream, HI_TRUE);

						audioframelen = 0;

                         stAudioStream.u32Seq++;

						 audio_preUTS = nowUTS;

              }
	}
	
	return HI_SUCCESS; //return  指执行到这里程序就全部释放了。
}


 
//#if 0   //if 0 ，后面的程序就是不执行的，不用搞那么多的'' // '' ,后续要用''end if  ''，作为结束。整段就取消了。
//duuiid
//#endif
//#ifdef __cplusplus
//#if __cplusplus
//}
//#endif
//#endif /* End of #ifdef __cplusplus */

