/******************************************************************************
h
  Copyright (C), 2009-2019,     Tech. Co., Ltd.

 ******************************************************************************
  File Name     : main.c
  Version       : Initial Draft
  Author        :    .com
  Created       : 2009/8/19
  Description   : 
  History       :
  1.Date        : 
    Author      : 
    Modification: Created file

******************************************************************************/
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
#include <sys/socket.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <netdb.h>
    
    
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>


#include "sample_comm.h"


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

int InitRtpUdpSock(int port, char *ipaddr, struct sockaddr_in *addr)
{
	int len;
	int sockid;
	struct sockaddr_in sockaddr;

	sockid = socket(AF_INET, SOCK_DGRAM, 0);

	
	bzero(&sockaddr,sizeof(sockaddr));
	sockaddr.sin_family=AF_INET;
	sockaddr.sin_addr.s_addr=inet_addr(ipaddr);//这里不一样
	sockaddr.sin_port=htons(port);
	

	char *buff= "dasddfsadfasfdsfsdf";
	len = sendto(sockid, buff, strlen(buff), 0, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	printf("len %d \n", len);
	memcpy(addr, &sockaddr, sizeof(sockaddr));
	return sockid;
}

int InitRtpTcpSock(int port, struct sockaddr* sintcpclientaddr)
{
	int sockid;
	socklen_t len;
	struct sockaddr_in sockaddr;
	int sockclient;
	
	sockid = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&sockaddr,sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(port); 

	if(bind( sockid, (struct sockaddr*)&sockaddr, sizeof(sockaddr))<0)
	{
		close(sockid);
		return 0;
	}

	if(listen(sockid, 5)<0)
	{
		close(sockid);
		return 0;
	}

        
#if 0
            int t =1;
            setsockopt(sockid, IPPROTO_TCP, TCP_NODELAY, &t, sizeof(int));
#endif

	//struct sockaddr_in sintcpclientaddr;
	len = sizeof(sintcpclientaddr);
	printf("wait accept...\n");
	sockclient = accept(sockid, (struct sockaddr*)&sintcpclientaddr, &len);

	char *bu= "ddsaaaaaaaaaaaaaaaaaaaaaaa";
	len = sizeof(sintcpclientaddr);
	//sendto(sockclient, bu, strlen(bu), 0, (struct sockaddr*)&sintcpclientaddr, sizeof(sintcpclientaddr));
	send(sockclient, bu, strlen(bu), 0);
	printf("already accept..\n");

	return sockclient;

}

#define SAMPLE_AUDIO_PTNUMPERFRM   320

#define SAMPLE_AUDIO_AI_DEV 0

#define SAMPLE_AUDIO_AO_DEV 2

#define SAMPLE_DBG(s32Ret)\
do{\
    printf("s32Ret=%#x,fuc:%s,line:%d\n", s32Ret, __FUNCTION__, __LINE__);\
}while(0)

static PAYLOAD_TYPE_E gs_enPayloadType = PT_ADPCMA;

static HI_BOOL gs_bMicIn = HI_FALSE;

static HI_BOOL gs_bAiAnr = HI_FALSE;
static HI_BOOL gs_bAioReSample = HI_FALSE;
static HI_BOOL gs_bUserGetMode = HI_FALSE;
static AUDIO_RESAMPLE_ATTR_S *gs_pstAiReSmpAttr = NULL;
static AUDIO_RESAMPLE_ATTR_S *gs_pstAoReSmpAttr = NULL;

HI_S32 Init_AENC_AUDIO()
{
    AIO_ATTR_S stAioAttr;

    
    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_8000;
     stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
     stAioAttr.enWorkmode = AIO_MODE_I2S_SLAVE;
     stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
     stAioAttr.u32EXFlag = 1;
     stAioAttr.u32FrmNum = 30;
     stAioAttr.u32PtNumPerFrm = SAMPLE_AUDIO_PTNUMPERFRM;
     stAioAttr.u32ChnCnt = 2;
     stAioAttr.u32ClkSel = 1;


     
    HI_S32 i, s32Ret;
    AUDIO_DEV   AiDev = SAMPLE_AUDIO_AI_DEV;
    AI_CHN      AiChn;
    AUDIO_DEV   AoDev = SAMPLE_AUDIO_AO_DEV;
    AO_CHN      AoChn = 0;
    ADEC_CHN    AdChn = 0;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn;

    FILE        *pfd = NULL;

   #if 0
   VB_CONF_S stVbConf;
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: system init failed with %d!\n", __FUNCTION__, s32Ret);
        return HI_FAILURE;
    }
#endif

    /********************************************
      step 1: config audio codec
    ********************************************/
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr, gs_bMicIn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    /********************************************
      step 2: start Ai
    ********************************************/
    s32AiChnCnt = stAioAttr.u32ChnCnt; 
    s32AencChnCnt = 1;//s32AiChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, gs_bAiAnr, gs_pstAiReSmpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    /********************************************
      step 3: start Aenc
    ********************************************/
    s32Ret = SAMPLE_COMM_AUDIO_StartAenc(s32AencChnCnt, gs_enPayloadType);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        return HI_FAILURE;
    }

    /********************************************
      step 4: Aenc bind Ai Chn
    ********************************************/
    for (i=0; i<s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        if (HI_TRUE == gs_bUserGetMode)
        {
            s32Ret = SAMPLE_COMM_AUDIO_CreatTrdAiAenc(AiDev, AiChn, AeChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_DBG(s32Ret);
                return HI_FAILURE;
            }
        }
        else
        {        
            s32Ret = SAMPLE_COMM_AUDIO_AencBindAi(AiDev, AiChn, AeChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_DBG(s32Ret);
                return s32Ret;
            }
        }
        printf("Ai(%d,%d) bind to AencChn:%d ok!\n",AiDev , AiChn, AeChn);
    }



    return HI_SUCCESS;

}



VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;

HI_S32 VENC_4D1_H264()
{
	SAMPLE_VI_MODE_E enViMode = SAMPLE_VI_MODE_4_D1;

	HI_U32 u32ViChnCnt = 4;
	HI_S32 s32VpssGrpCnt = 4;
	PAYLOAD_TYPE_E enPayLoad[2]= {PT_H264, PT_H264};
	PIC_SIZE_E enSize[2] = {PIC_D1, PIC_CIF};
	
	VB_CONF_S stVbConf;
	VPSS_GRP VpssGrp;
	VPSS_CHN VpssChn;
	VPSS_GRP_ATTR_S stGrpAttr;
	VENC_GRP VencGrp;
	VENC_CHN VencChn;
	SAMPLE_RC_E enRcMode;
	
	HI_S32 i;
	HI_S32 s32Ret = HI_SUCCESS;
	HI_U32 u32BlkSize;
	HI_CHAR ch;
	SIZE_S stSize;

	/******************************************
	 step  1: init variable 
	******************************************/
	memset(&stVbConf,0,sizeof(VB_CONF_S));

	u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
				PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);
	stVbConf.u32MaxPoolCnt = 128;
	
	stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
	stVbConf.astCommPool[0].u32BlkCnt = u32ViChnCnt * 6;
	memset(stVbConf.astCommPool[0].acMmzName,0,
		sizeof(stVbConf.astCommPool[0].acMmzName));

	/* hist buf*/
	stVbConf.astCommPool[1].u32BlkSize = (196*4);
	stVbConf.astCommPool[1].u32BlkCnt = u32ViChnCnt * 6;
	memset(stVbConf.astCommPool[1].acMmzName,0,
		sizeof(stVbConf.astCommPool[1].acMmzName));

	/******************************************
	 step 2: mpp system init. 
	******************************************/
	s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("system init failed with %d!\n", s32Ret);
		goto END_VENC_8D1_0;
	}

	/******************************************
	 step 3: start vi dev & chn to capture
	******************************************/
	s32Ret = SAMPLE_COMM_VI_Start(enViMode, gs_enNorm);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("start vi failed!\n");
		goto END_VENC_8D1_0;
	}
	
	/******************************************
	 step 4: start vpss and vi bind vpss
	******************************************/
	s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, PIC_D1, &stSize);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
		goto END_VENC_8D1_0;
	}
	
	stGrpAttr.u32MaxW = stSize.u32Width;
	stGrpAttr.u32MaxH = stSize.u32Height;
	stGrpAttr.bDrEn = HI_FALSE;
	stGrpAttr.bDbEn = HI_FALSE;
	stGrpAttr.bIeEn = HI_TRUE;
	stGrpAttr.bNrEn = HI_TRUE;
	stGrpAttr.bHistEn = HI_TRUE;
	stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
	stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

	s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Vpss failed!\n");
		goto END_VENC_8D1_1;
	}

	s32Ret = SAMPLE_COMM_VI_BindVpss(enViMode);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Vi bind Vpss failed!\n");
		goto END_VENC_8D1_2;
	}

	/******************************************
	 step 5: select rc mode
	******************************************/
	//while(1)
	{
		//printf("please choose rc mode:\n"); 
		//printf("\t0) CBR\n"); 
		//printf("\t1) VBR\n"); 
		//printf("\t2) FIXQP\n"); 
		ch = '1';
	
		if ('0' == ch)
		{
			enRcMode = SAMPLE_RC_CBR;
		}
		else if ('1' == ch)
		{
			enRcMode = SAMPLE_RC_VBR;
		}
		else if ('2' == ch)
		{
			enRcMode = SAMPLE_RC_FIXQP;
		}
		else
		{
			printf("rc mode invaild! please try again.\n");
		}
	}
	/******************************************
	 step 5: start stream venc (big + little)
	******************************************/
	for (i=0; i<u32ViChnCnt; i++)
	{
		/*** main stream **/
		VencGrp = i*2;
		VencChn = i*2;
		VpssGrp = i;
		s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[0],\
									   gs_enNorm, enSize[0], enRcMode);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("Start Venc failed!\n");
			goto END_VENC_8D1_2;
		}

		s32Ret = SAMPLE_COMM_VENC_BindVpss(VencGrp, VpssGrp, VPSS_BSTR_CHN);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("Start Venc failed!\n");
			goto END_VENC_8D1_3;
		}

		/*** Sub stream **/
		VencGrp ++;
		VencChn ++;
		s32Ret = SAMPLE_COMM_VENC_Start(VencGrp, VencChn, enPayLoad[1], \
										gs_enNorm, enSize[1], enRcMode);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("Start Venc failed!\n");
			goto END_VENC_8D1_3;
		}

		s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn, VpssGrp, VPSS_PRE0_CHN);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("Start Venc failed!\n");
			goto END_VENC_8D1_3;
		}
	}

	return;

	/******************************************
	 step 6: stream venc process -- get stream, then save it to file. 
	******************************************/
	s32Ret = SAMPLE_COMM_VENC_StartGetStream(u32ViChnCnt*2);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Start Venc failed!\n");
		goto END_VENC_8D1_3;
	}

	printf("please press twice ENTER to exit this sample\n");
	getchar();
	getchar();

	/******************************************
	 step 7: exit process
	******************************************/
	SAMPLE_COMM_VENC_StopGetStream();
	
END_VENC_8D1_3:
	for (i=0; i<u32ViChnCnt*2; i++)
	{
		VencGrp = i;
		VencChn = i;
		VpssGrp = i/2;
		VpssChn = (VpssGrp%2)?VPSS_PRE0_CHN:VPSS_BSTR_CHN;
		SAMPLE_COMM_VENC_UnBindVpss(VencGrp, VpssGrp, VpssChn);
		SAMPLE_COMM_VENC_Stop(VencGrp,VencChn);
	}
	SAMPLE_COMM_VI_UnBindVpss(enViMode);
END_VENC_8D1_2: //vpss stop
	SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
END_VENC_8D1_1: //vi stop
	SAMPLE_COMM_VI_Stop(enViMode);
END_VENC_8D1_0: //system exit
	SAMPLE_COMM_SYS_Exit();
	
	return s32Ret;
}


#if 0
void InitAndStartEncode()
{
        MPP_SYS_CONF_S stSysConf = {0};
        VB_CONF_S stVbConf;
        int   u32ViChnCnt  = 4;
        HI_S32 s32Ret = HI_FAILURE;
        
        memset(&stVbConf,0,sizeof(VB_CONF_S));
        
        stVbConf.u32MaxPoolCnt = 128;
            
        stVbConf.astCommPool[0].u32BlkSize = 768*576*3/2;
        stVbConf.astCommPool[0].u32BlkCnt = u32ViChnCnt * 6;
        memset(stVbConf.astCommPool[0].acMmzName,0,
            sizeof(stVbConf.astCommPool[0].acMmzName));
    
        /* hist buf*/
        stVbConf.astCommPool[1].u32BlkSize = (196*4);
        stVbConf.astCommPool[1].u32BlkCnt = u32ViChnCnt * 6;
        memset(stVbConf.astCommPool[1].acMmzName,0,
        sizeof(stVbConf.astCommPool[1].acMmzName));

        
         HI_MPI_SYS_Exit();
         HI_MPI_VB_Exit();

         s32Ret = HI_MPI_VB_SetConf(&stVbConf);

         s32Ret = HI_MPI_VB_Init();

         stSysConf.u32AlignWidth = SAMPLE_SYS_ALIGN_WIDTH;
         s32Ret = HI_MPI_SYS_SetConf(&stSysConf);

         s32Ret = HI_MPI_SYS_Init();

         

#if 1
	VIDEO_PREPROC_CONF_S stConf;
	VENC_ATTR_H264_S stH264Attr; //创建编码通道需要的参数
	VENC_CHN_ATTR_S stAttr;
	
	int index = 0;
	

//////////////////////////////////////////////////////////
	MPP_SYS_CONF_S stSysConf = {0};
	VB_CONF_S stVbConf ={0};
	
	HI_MPI_SYS_Exit();
	HI_MPI_VB_Exit();

	stVbConf.astCommPool[0].u32BlkSize = 768*576*3/2;//D1:704*576*2;
	stVbConf.astCommPool[0].u32BlkCnt = 10;

	HI_MPI_VB_SetConf(&stVbConf);//设置MPP视频缓存池属性
	HI_MPI_VB_Init();//初始化MPP视频缓存池

	stSysConf.u32AlignWidth = 64;
	HI_MPI_SYS_SetConf(&stSysConf);//配置系统控制参数
	HI_MPI_SYS_Init();//初始化MPP系统

	VI_PUB_ATTR_S ViAttr;
	memset(&ViAttr, 0, sizeof(VI_PUB_ATTR_S));
	ViAttr.enInputMode = VI_MODE_DIGITAL_CAMERA;
	ViAttr.enWorkMode = VI_WORK_MODE_1D1;
	ViAttr.u32AdType = AD_2864;
	ViAttr.enViNorm = VIDEO_ENCODING_MODE_PAL;
	HI_MPI_VI_SetPubAttr(0, &ViAttr);//设置VI设备属性
	HI_MPI_VI_Enable(0);//使能启用VI设备

	VI_CHN_ATTR_S ViChnAttr;
	ViChnAttr.bHighPri = HI_FALSE;
	ViChnAttr.bChromaResample = HI_FALSE;
	ViChnAttr.bDownScale = HI_FALSE;
	ViChnAttr.enCapSel = VI_CAPSEL_BOTH;

	
	ViChnAttr.enViPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
		
		
	ViChnAttr.stCapRect.s32X = 8;//解决左右边框黑线=8(demo板)
	ViChnAttr.stCapRect.s32Y = 0;
	//ViChnAttr.stCapRect.u32Width = 720;
	ViChnAttr.stCapRect.u32Width = 704;
	ViChnAttr.stCapRect.u32Height = 576;
	HI_MPI_VI_SetChnAttr(0, 0, &ViChnAttr);//设置VI通道属性
	HI_MPI_VI_EnableChn(0, 0);//启用VI通道
	//system("rmmod myregread.ko");
	//system("insmod ./myregread.ko reg=0 val=0x40c1a8");
	system("himm 0x90060000 0x0040c1a8");//YUV设置  不然图片模糊不清  ， 黑白
	

/////////////////////////////////////////////////////////

	HI_MPI_VPP_GetConf(index, &stConf);//获取视频前处理配置

	stConf.s32SrcFrmRate = 25;
	stConf.s32TarFrmRate = 25;

	HI_MPI_VPP_SetConf(index, &stConf);//设置视频前处理配置

	stH264Attr.u32Priority = 0;
	stH264Attr.bMainStream = HI_TRUE;
	stH264Attr.u32MaxDelay = 200;
	stH264Attr.bByFrame = HI_TRUE;/*按帧获取*/
	stH264Attr.bField = HI_FALSE;/*帧编码*/

	
	stH264Attr.u32PicLevel = 2;
	stH264Attr.bCBR = HI_FALSE;
	stH264Attr.u32Bitrate = 768*2;
	stH264Attr.u32Gop = 25;
	stH264Attr.u32ViFramerate = 25;

	stH264Attr.bVIField = HI_FALSE;/*帧*/
	stH264Attr.u32PicWidth = 704;

	stH264Attr.u32PicHeight = 576;
	stH264Attr.u32BufSize = 704*576*3/2*3/2;//
	stH264Attr.u32TargetFramerate =25;

	memset(&stAttr, 0 ,sizeof(VENC_CHN_ATTR_S));
	stAttr.enType = PT_H264;
	stAttr.pValue = (HI_VOID *)&stH264Attr;


	HI_MPI_VENC_CreateGroup(0);//创建编码通道组
	HI_MPI_VENC_CreateChn(0, &stAttr, HI_NULL);//创建编码通道

	HI_MPI_VENC_RegisterChn(0, 0);//注册编码通道到通道组
	HI_MPI_VENC_BindInput(0, 0, 0);//绑定VI到通道组

	HI_MPI_VENC_StartRecvPic(0);//开启编码通道接收输入图像
#endif
#if 0
	unsigned char *buffer = NULL;
	FrameBuffer	en_f_buf;

	en_f_buf.bufferstart= (unsigned char *)malloc(buffersize);

	memset(en_f_buf.bufferstart, 0, buffersize);	

	en_f_buf.buffersize = buffersize;
	en_f_buf.readpos = 0;
	en_f_buf.writepos = 0;
	en_f_buf.Ilistpos = 0;
#endif

}
#endif

#define IFRAME_MAX_NUM 32

#define BUFFER_SIZE 8*1024*1024

typedef struct _Iframe_list
{
     unsigned long iframe_offset;                 /*I 帧的地址偏移索引*/

     unsigned long long  time_stamp;                    /*记录I帧的时间戳*/
   
}Iframe_list;


typedef struct _frame_buffer
{
        unsigned char *buffer_start;                 /*视频缓冲的起始地址*/
        unsigned long buffer_size;                    /*视频缓冲大小*/                                       /*10M*/
        unsigned long read_offset;                    /*视频缓冲的读偏移*/
        unsigned long write_offset;                   /*视频缓冲的写偏移*/

        Iframe_list ikey_list[IFRAME_MAX_NUM];                  /*视频缓冲中保存的I帧索引及其时间*/
                                                                                                  /*32 个 I 帧的时间差多少 ??*/
        int Ilist_curr;                                          /*最新一个写入缓冲的I 帧索引*/
        

        //pthread_mutex_t buff_mutex; /*写缓冲互斥锁   */
        
}Frame_buffer;

struct _frame_buffer *frame_buff;  

pthread_mutex_t write_mutex;


void  Init_Frame_Buffer(int num)
{
	int index = 0;
	int i_index = 0;

	if(num < 0)
		return ;
	
	frame_buff = (struct _frame_buffer  *)malloc(num * sizeof(Frame_buffer));

	for(index = 0; index < num; index++)
	{
		frame_buff[index].buffer_size = BUFFER_SIZE;
		//printf("buffer size = %ld \n", frame_buff[index].buffer_size);

		frame_buff[index].buffer_start = (unsigned char *)malloc(frame_buff[index].buffer_size);
		if(frame_buff[index].buffer_start == NULL)
		{
			printf("malloc video buffer failed!\n");
		}

		frame_buff[index].read_offset = 0;
		frame_buff[index].write_offset = 0;
		frame_buff[index].Ilist_curr = 0;

		for(i_index = 0; i_index < IFRAME_MAX_NUM; i_index++)
		{
			memset(&(frame_buff[index].ikey_list[i_index]), 0, sizeof(Iframe_list));
		}
	}
	
}

//保存视频数据
void  Save_Frame_Data(void *p, int channel) 
{
		//printf("Save_Frame_Data\n");

        int is_ikey = 0;
	int i_num, index = 0;
        unsigned char frameId = 0;
        unsigned int frame_len = 0;
        unsigned long long frame_pts = 0;
        unsigned  char *frame_data;

        unsigned int IFrameType = {0x63643030};
	unsigned int PFrameType = {0x63643130};
    
        VENC_STREAM_S *stream = NULL;
        VENC_PACK_S *packet = NULL;

        pthread_mutex_lock(&write_mutex);//写buffer 锁

        unsigned long write_offset_tmp = frame_buff[channel].write_offset; 

		//printf("write offset %ld \n",write_offset_tmp);

        if(write_offset_tmp >= frame_buff[channel].buffer_size)
		{	
			write_offset_tmp = 0;
		}

        stream = (VENC_STREAM_S *)p;

        if(stream->pstPack)
		{
			if(stream->pstPack[0].u32Len[0] > 4)
			{
				frameId = *(stream->pstPack[0].pu8Addr[0] + 4);
				
			}
			else
			{
				unsigned int index = 4 - stream->pstPack[0].u32Len[0];
				if(stream->pstPack[0].u32Len[1] > index)
				{
					frameId = *(stream->pstPack[0].pu8Addr[1] + index);
				}
				else
				{
					;//return -1;
				}
			}

		}
		else
		{
			return -1;
		}

		//printf("frame id %02x  ", frameId);
		
      	if(stream->pstPack)
		{
			for(index = 0; index < stream->u32PackCount; index ++)
			{
				packet = stream->pstPack + index;
				frame_pts = packet->u64PTS;
				frame_len += (packet->u32Len[0] + packet->u32Len[1]);
			}
		}
    //printf("len = %d frame write offset **=  %ld  ",frame_len, write_offset_tmp);

	/*是否 I 帧视频数据， 记录I帧时间及缓冲中偏移地址*/
	if(frameId == 0x67)                  
	{
		if(frame_buff[channel].Ilist_curr < (IFRAME_MAX_NUM - 1))   /*默认I 帧索引数组中保存32个I 帧，循环*/
		{
			frame_buff[channel].Ilist_curr++;
		}
		else
		{
			frame_buff[channel].Ilist_curr = 0;
		}
		
		i_num = frame_buff[channel].Ilist_curr;

		frame_buff[channel].ikey_list[i_num].iframe_offset = write_offset_tmp;   /*记录此I 帧的视频数据偏移量*/
		frame_buff[channel].ikey_list[i_num].time_stamp = frame_pts;   			 /*记录此I 帧的时间还是记录??*/

                (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp)) = IFrameType;//I 帧类型   4个字节
	}
        else
        {
                (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp))= PFrameType;  //P帧类型   4个字节
        }

        (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp + 4)) = frame_len;  //帧长度  4个字节
		//printf("frame_len = %d  %d\n", (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp + 4)),frame_len);
        write_offset_tmp  += 8;
        if(write_offset_tmp >= frame_buff[channel].buffer_size)
	{	
		write_offset_tmp = 0;
	}

         (*(unsigned long long *)(frame_buff[channel].buffer_start + write_offset_tmp)) = frame_pts;  //添加时间戳  8个字节
        write_offset_tmp  += 8;
        if(write_offset_tmp>= frame_buff[channel].buffer_size)
	{	
		write_offset_tmp = 0;
	}

        /*将此帧视频数据保存到缓冲中*/

	/*比较缓冲中剩余的大小和此视频数据的大小*/

        int left_size = 0;
        if(stream->pstPack)
       {
		for (index=0; index < stream->u32PackCount; ++index)
		{	
			if(stream->pstPack[index].u32Len[0] && \
						stream->pstPack[index].pu8Addr[0])
			{
				left_size = frame_buff[channel].buffer_size - write_offset_tmp;
				if ((int)stream->pstPack[index].u32Len[0] <= left_size)
				{
					memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pstPack[index].pu8Addr[0], stream->pstPack[index].u32Len[0]);
					write_offset_tmp += stream->pstPack[index].u32Len[0];
				}
				else
				{
					memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pstPack[index].pu8Addr[0], left_size);
					memcpy(frame_buff[channel].buffer_start, ((unsigned char *)stream->pstPack[index].pu8Addr[0])+left_size, stream->pstPack[index].u32Len[0] - left_size);
					write_offset_tmp = (stream->pstPack[index].u32Len[0] - left_size);
				}
			}
				
			if(stream->pstPack[index].u32Len[1] && \
						stream->pstPack[index].pu8Addr[1])
			{
				left_size = frame_buff[channel].buffer_size - write_offset_tmp;
				if ((int)stream->pstPack[index].u32Len[1] <= left_size)
				{
					memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pstPack[index].pu8Addr[1], stream->pstPack[index].u32Len[1]);
					write_offset_tmp += stream->pstPack[index].u32Len[1];
				}
				else
				{
					memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pstPack[index].pu8Addr[1], left_size);
					memcpy(frame_buff[channel].buffer_start, ((unsigned char *)stream->pstPack[index].pu8Addr[1])+left_size, stream->pstPack[index].u32Len[1] - left_size);
					write_offset_tmp = (stream->pstPack[index].u32Len[1] - left_size);
				}
			}								
		}
	}

#if 0
        //清除后面未对齐的字节
        int temp = 8 - write_offset_tmp %8;
        if(temp > 0)
        {
        	memset(frame_buff[channel].buffer_start + write_offset_tmp, 0, temp);
        	write_offset_tmp += temp;//八字节对齐
        	if(write_offset_tmp >= frame_buff[channel].buffer_size)
        	{	
        		write_offset_tmp = 0;
        	}
            
        }
#endif
        frame_buff[channel].write_offset = write_offset_tmp;
		//printf("end write offset **=  %ld\n", write_offset_tmp);
        pthread_mutex_unlock(&write_mutex);
        
}


//保存音频数据
void Save_Audio_Data(void *p,  int channel)
{
        unsigned int frametype = 0x62773330;
        AUDIO_STREAM_S *stream = NULL;
        unsigned int frame_len;
        unsigned long long frame_pts = 0;
        
        //帧缓冲中添加帧类型 4    帧长度 4   时间戳8

        pthread_mutex_lock(&write_mutex);//写buffer 锁

        unsigned long write_offset_tmp = frame_buff[channel].write_offset; 
        
        stream = (AUDIO_STREAM_S *)p;

        if(write_offset_tmp >= frame_buff[channel].buffer_size)
	{	
		write_offset_tmp = 0;
	}

        frame_len = stream->u32Len;
        (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp)) = frametype;  //P帧类型   4个字节

        (*(unsigned int *)(frame_buff[channel].buffer_start + write_offset_tmp + 4)) = frame_len;  //帧长度  4个字节

        write_offset_tmp  += 8;
        if(write_offset_tmp >= frame_buff[channel].buffer_size)
	{	
		write_offset_tmp = 0;
	}

        frame_pts = stream->u64TimeStamp;
         (*(unsigned long long *)(frame_buff[channel].buffer_start + write_offset_tmp + 4)) = frame_pts;
        write_offset_tmp  += 8;
        if(write_offset_tmp >= frame_buff[channel].buffer_size)
	{	
		write_offset_tmp = 0;
	}

        unsigned int left_size = 0;
        left_size = frame_buff[channel].buffer_size - write_offset_tmp;
        if ((unsigned int)stream->u32Len <= left_size)
	{
		memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pStream, stream->u32Len);
		write_offset_tmp += stream->u32Len;
	}
	else
	{
		memcpy(frame_buff[channel].buffer_start + write_offset_tmp, stream->pStream, left_size);
		memcpy(frame_buff[channel].buffer_start, stream->pStream+left_size, stream->u32Len - left_size);
		write_offset_tmp = (stream->u32Len - left_size);
	}

         //清除后面未对齐的字节
        int temp = 8 - write_offset_tmp %8;
        if(temp > 0)
        {
        	memset(frame_buff[channel].buffer_start + write_offset_tmp, 0, temp);
        	write_offset_tmp += temp;//八字节对齐
        	if(write_offset_tmp >= frame_buff[channel].buffer_size)
        	{	
        		write_offset_tmp = 0;
        	}
            
        }

        frame_buff[channel].write_offset  = write_offset_tmp;
        pthread_mutex_unlock(&write_mutex);

}



void GetAudioStream()
    {
        HI_S32 s32Ret;
        HI_S32 AencFd;

        AUDIO_STREAM_S stStream;
        HI_S32  channel = 0;                    //0 channel
        
        fd_set read_fds;
        struct timeval TimeoutVal;
        
        FD_ZERO(&read_fds);    
        AencFd = HI_MPI_AENC_GetFd(channel);
        FD_SET(AencFd, &read_fds);
        
        while (1)
        {     
            TimeoutVal.tv_sec = 1;
            TimeoutVal.tv_usec = 0;
            
            FD_ZERO(&read_fds);
            FD_SET(AencFd,&read_fds);
            
            s32Ret = select(AencFd+1, &read_fds, NULL, NULL, &TimeoutVal);
            if (s32Ret < 0) 
            {
                printf("%s: get aenc stream select error\n", __FUNCTION__);
                break;
            }
            else if (0 == s32Ret) 
            {
                printf("%s: get aenc stream select time out\n", __FUNCTION__);
                ;
            }
            
            if (FD_ISSET(AencFd, &read_fds))
            {
                /* get stream from aenc chn */
                s32Ret = HI_MPI_AENC_GetStream(channel, &stStream, HI_FALSE);
                if (HI_SUCCESS != s32Ret )
                {
                       continue;
                }

                Save_Audio_Data(&stStream,  channel);//保存音频数据到buffer中
    
                /* send stream to decoder and play for testing */
                 //HI_MPI_ADEC_SendStream(channel, &stStream, HI_TRUE);

                
                /* finally you must release the stream */
                HI_MPI_AENC_ReleaseStream(channel, &stStream);
            }    
        }
        
        return NULL;
    }


void GetVideoStream()
{
	HI_S32 i;
	HI_S32 s32ChnTotal;
	VENC_CHN_ATTR_S stVencChnAttr;
	SAMPLE_VENC_GETSTREAM_PARA_S pstPara;
	HI_S32 maxfd = 0;
	struct timeval TimeoutVal;
	fd_set read_fds;
	HI_S32 VencFd[VENC_MAX_CHN_NUM];
	HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
	FILE *pFile[VENC_MAX_CHN_NUM];
	char szFilePostfix[10];
	VENC_CHN_STAT_S stStat;
	VENC_STREAM_S stStream;
	HI_S32 s32Ret;
	VENC_CHN VencChn;
	PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
	
	pstPara.s32Cnt = 1;
	pstPara.bThreadStart = HI_TRUE;
	s32ChnTotal = pstPara.s32Cnt;

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
		VencChn = i;
		s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
		if(s32Ret != HI_SUCCESS)
		{
			SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
				   VencChn, s32Ret);
			return NULL;
		}
		enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

		s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
		if(s32Ret != HI_SUCCESS)
		{
			SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
				   stVencChnAttr.stVeAttr.enType, s32Ret);
			return NULL;
		}
		sprintf(aszFileName[i], "stream_chn%d%s", i, szFilePostfix);
		pFile[i] = fopen(aszFileName[i], "wb");
		if (!pFile[i])
		{
			SAMPLE_PRT("open file[%s] failed!\n", 
				   aszFileName[i]);
			return NULL;
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
	}

	/******************************************
	 step 2:  Start to get streams of each channel.
	******************************************/
	while (HI_TRUE == pstPara.bThreadStart)
	{
		FD_ZERO(&read_fds);
		for (i = 0; i < s32ChnTotal; i++)
		{
			FD_SET(VencFd[i], &read_fds);
		}

		TimeoutVal.tv_sec  = 2;
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
					s32Ret = HI_MPI_VENC_Query(i, &stStat);
					if (HI_SUCCESS != s32Ret)
					{
						SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
						break;
					}

					/*******************************************************
					 step 2.2 : malloc corresponding number of pack nodes.
					*******************************************************/
					stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
					if (NULL == stStream.pstPack)
					{
						SAMPLE_PRT("malloc stream pack failed!\n");
						break;
					}
					
					/*******************************************************
					 step 2.3 : call mpi to get one-frame stream
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
					 step 2.4 : save frame to file
					*******************************************************/
					s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i], &stStream);
					if (HI_SUCCESS != s32Ret)
					{
						free(stStream.pstPack);
						stStream.pstPack = NULL;
						SAMPLE_PRT("save stream failed!\n");
						break;
					}

					Save_Frame_Data(&stStream, 0) ;
					
					/*******************************************************
					 step 2.5 : release stream
					*******************************************************/
					s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
					if (HI_SUCCESS != s32Ret)
					{
						free(stStream.pstPack);
						stStream.pstPack = NULL;
						break;
					}
					/*******************************************************
					 step 2.6 : free pack nodes
					*******************************************************/
					free(stStream.pstPack);
					stStream.pstPack = NULL;
				}
			}
		}
	}

	/*******************************************************
	* step 3 : close save-file
	*******************************************************/
	for (i = 0; i < s32ChnTotal; i++)
	{
		fclose(pFile[i]);
	}

	return NULL;
}

#if 0
void TMP()
{
	VENC_CHN channel = 0;
	int VencFd = 0;
	fd_set read_fds;
	int s32ret = -1;
	int s32Ret;
	VENC_STREAM_S stVStream;
	VENC_CHN_STAT_S stStat;
	unsigned int PacketIndex = 0;

	struct timeval TimeoutVal;
	
	VENC_CHN VencChn = 0;
	VENC_CHN_ATTR_S stVencChnAttr;

	s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
               VencChn, s32Ret);
        return NULL;
    }
    enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

    s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
               stVencChnAttr.stVeAttr.enType, s32Ret);
        return NULL;
    }
	
	VencFd = HI_MPI_VENC_GetFd(channel);//获取编码通道对应的设备文件句柄

        while(1)
        {
        	FD_ZERO(&read_fds);
        	FD_SET(VencFd,&read_fds);
			TimeoutVal.tv_sec  = 2;
        	TimeoutVal.tv_usec = 0;
        	
        	s32ret = select(VencFd + 1,&read_fds,NULL,NULL,&TimeoutVal);
			if (s32ret < 0)
	        {
	            SAMPLE_PRT("select failed!\n");
	            break;
	        }
	        else if (s32ret == 0)
	        {
	            SAMPLE_PRT("get venc stream time out, exit thread\n");
	            continue;
	        }
        	printf("&&&&&&&&&&&&&&&&&&************\n");

        	if(FD_ISSET(VencFd,&read_fds))
        	{
        		memset(&stVStream,0,sizeof(stVStream));
        		s32ret = HI_MPI_VENC_Query(channel, &stStat);//查询编码通道状态

        		stVStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S)*stStat.u32CurPacks);
        		stVStream.u32PackCount = stStat.u32CurPacks;

        		s32ret = HI_MPI_VENC_GetStream(channel, &stVStream, HI_TRUE);//获取编码码流

        		if((s32ret != 0) && stVStream.pstPack)//保存视频码流数据
        		{	
        			Save_Frame_Data(&stVStream, channel) ;
					printf("Save_Frame_Data\n");
        		}
                
        	}
                s32ret = HI_MPI_VENC_ReleaseStream(channel,&stVStream);//释放码流缓存
       }
}
#endif

#if 0
#define MCAST_ADDR "224.0.0.100"


int Init_Multicast_Sock(struct sockaddr_in *addr)
{
	printf("#####################################\n");
	

	int s;
	struct sockaddr_in mcast_addr;
	s = socket(AF_INET, SOCK_DGRAM, 0);/*建立套接字zxb*/
	if (s == -1)
	{
		perror("socket()");
		return -1;
	}

	memset(&mcast_addr, 0, sizeof(mcast_addr));/*初始化IP多播地址为0 zxb */
	mcast_addr.sin_family = AF_INET;/*设置协议族类行为AF*/
	mcast_addr.sin_addr.s_addr = inet_addr(MCAST_ADDR);/*设置多播IP地址zxb*/
	mcast_addr.sin_port = htons(8888);/*设置多播端口zxb*/

	char buff[64] = "BROADCAST TEST DATA";
	sendto(s, buff,sizeof(buff), 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr)) ;/*发送数据到组播地址224.0.0.100中*/
	printf("#####################################\n");
	memcpy(addr, &mcast_addr, sizeof(mcast_addr));
	return s;
}
#endif

typedef struct  CurFrameInfo
{
	unsigned int frame_length;
        unsigned int frame_type;
        unsigned long frame_read_offset;
        unsigned long long frame_pts;
}CurFrameInfo;


int  Get_Frame_Info(CurFrameInfo *pcurframe, int channel)/**/
{
        int i_curr;
        int leave_len;
        unsigned int frame_len;
        unsigned long read_pos;
        
        
        unsigned long write_offset_tmp = frame_buff[channel].write_offset; 

		if(frame_buff[channel].read_offset == 0)
        {
        	i_curr = frame_buff[channel].Ilist_curr;
        	frame_buff[channel].read_offset = frame_buff[channel].ikey_list[i_curr].iframe_offset;
		}

        //printf("get data ~\n");
        if(frame_buff[channel].read_offset >= frame_buff[channel].buffer_size)
        {   
            frame_buff[channel].read_offset = 0;
        }

		//printf("read offset %ld \n",frame_buff[channel].read_offset);
        
        if (write_offset_tmp == frame_buff[channel].read_offset)//读等于写，等待写数据 返回0 个字节; 
        { 
            //printf("waiting write frame data!\n"); 
            usleep(1000*20);
            return 0; 
        } 

        //pcurframe->frame_read_offset = frame_buff[channel].read_offset;
        pcurframe->frame_type= (*(unsigned int *)(frame_buff[channel].buffer_start + frame_buff[channel].read_offset));

        pcurframe->frame_length = (*(unsigned int *)(frame_buff[channel].buffer_start + frame_buff[channel].read_offset + 4));
		//printf("read frame_length %ld ",pcurframe->frame_length);

        frame_buff[channel].read_offset += 8;
        if(frame_buff[channel].read_offset >= frame_buff[channel].buffer_size)
        {   
            frame_buff[channel].read_offset = 0;
        }

        pcurframe->frame_pts = (*(unsigned long long *)(frame_buff[channel].buffer_start + frame_buff[channel].read_offset));

        frame_buff[channel].read_offset += 8;
        if(frame_buff[channel].read_offset >= frame_buff[channel].buffer_size)
        {   
            frame_buff[channel].read_offset = 0;
        }

        pcurframe->frame_read_offset = frame_buff[channel].read_offset ;

        int off = 0;
#if 0

        if((pcurframe->frame_length %8)   > 0)//
	{
		off = pcurframe->frame_length%8;
		off = 8- off;
	}
#endif

        leave_len= frame_buff[channel].buffer_size - frame_buff[channel].read_offset;

        int tmp_frame_len = pcurframe->frame_length + off;
		//printf("frame len = %d \n",tmp_frame_len);
        
        if(tmp_frame_len < leave_len)
                frame_buff[channel].read_offset += tmp_frame_len;
        else if(tmp_frame_len >= leave_len)
                frame_buff[channel].read_offset = tmp_frame_len - leave_len;
        

        return 1;
}



void *Save_Video_Pthread(void *arg)
{
    GetVideoStream();
    pthread_exit(NULL);
}

void *Save_Audio_Pthread(void *arg)
{
    GetAudioStream();
    pthread_exit(NULL);
}
/*
unsigned char *CreateBufferPool(int bufsize)
{
	unsigned char *buffer = NULL;

	buffer= (unsigned char *)malloc(bufsize);
	if(buffer == NULL)
	{
		printf("malloc memory fail\n");
		return NULL;
	}

	memset(buffer,0,bufsize);	

	return buffer;
}

int CreateCommonBuffer(int channel, int buffersize)
{
	int status = 0;

	
	frame_buff[channel].buffer_start = CreateBufferPool(buffersize);
	if(NULL == frame_buff[channel].buffer_start)
	{
		fprintf(stderr,"have not enough memory!fatal error!");
		return -1;
	}
	
	
	frame_buff[channel].buffer_size = buffersize;
	frame_buff[channel].read_offset = 0;
	frame_buff[channel].write_offset = 0;
	//frame_buff[channel].ireadpos = 0;

	
	frame_buff[channel].Ilist_curr = 0;

	//en_buffer[ch][streamtype] = en_f_buf[ch][streamtype].bufferstart;

	return status;
}
*/

/***********************************************
功           能:	程序入口
输入参数:	系统自动调配
输出参数:	无
返  回   值:	无
*************************************************/
int main(int argc, char * argv[])
{

	/******************把所有的处理放到main函数*******************************/
    int i, ret;
	unsigned char *framebuff;
	unsigned int  buffersize = 512*1024;
	unsigned long long FramePts, prePts = 0;
	unsigned int timestamp_increse = 0;
	unsigned int framelen;
	RTP_FIXED_HEADER  *rtp_hdr;
	struct timeval now_tmp;
	int ssrc;
	unsigned int Rtp_seq_num = 0;
	unsigned char sendbuff[1500];
	int spi_frame_num;
	unsigned long UTS = 0;
	
	int sockclient;
//	int len, ret;
	//char recvbuf[1024] = {'\0'};

	struct sockaddr_in sintcpclientaddr;

        pthread_t video_thread_id;
        pthread_t audio_thread_id;

        CurFrameInfo frameInfo;

	Init_Frame_Buffer(1);

    //初始化以及启动编码设置//////
	//InitAndStartEncode();
	
	VENC_4D1_H264();
        Init_AENC_AUDIO();

        pthread_create(&video_thread_id, NULL, Save_Video_Pthread,NULL);
	//sleep(1);
		pthread_create(&audio_thread_id, NULL, Save_Audio_Pthread,NULL);
	////////////////////////////////////////////////

	//sockclient = Init_Multicast_Sock(&sintcpclientaddr);                                            //组播传输视频数据
	//sockclient = InitRtpTcpSock(8888, (struct sockaddr*)&sintcpclientaddr);                //TCP传输视频数据
	sockclient= InitRtpUdpSock(8888, "192.168.1.20", &sintcpclientaddr);   //UDP传输视频数据

	printf("##########################\n");

	framebuff = (unsigned char *)malloc(buffersize);//申请内存大小


	gettimeofday(&now_tmp, 0);//获取时间
    srand((now_tmp.tv_sec * 1000) + (now_tmp.tv_usec / 1000));
	ssrc = rand();//生成随机数
	//rtp帧头数据
	rtp_hdr =(RTP_FIXED_HEADER*)&sendbuff[0]; 
	rtp_hdr->payload = 96; //H264  
	rtp_hdr->version = 2;  
	rtp_hdr->padding = 0;
	rtp_hdr->extension =0;
	rtp_hdr->marker = 0;   
	rtp_hdr->csrc_len = 0;
	rtp_hdr->ssrc = htonl(ssrc); //随机指定的
	printf("##########################\n");
	int index;
	int sendnum;
	HI_MPI_VENC_RequestIDR(0);//请求编码I帧
	usleep(500*1000);
	while(1)
	{	
		//continue;
		framelen = 0;
		
		//GetVideoStream(framebuff, &framelen, &FramePts);//获取一帧数据、长度、时间戳 

                ret = Get_Frame_Info(&frameInfo, 0);
                if(ret ==0)
                    continue;

                if(0x62773330 == frameInfo.frame_type)/*音频*/
                {        
                        rtp_hdr->payload = 98;
                        continue; /*暂时调试视频*/
                }
                else
                {
                        rtp_hdr->payload = 96;

                }

                FramePts = frameInfo.frame_pts;
                framelen= frameInfo.frame_length;

				printf("len = %d pts = %lld \n", framelen, FramePts);
	        
		if(prePts != 0)
		{
			timestamp_increse =(unsigned int)(FramePts - prePts); //时间戳增量
		}
		prePts = FramePts;

		UTS = UTS + timestamp_increse;
		rtp_hdr->timestamp = htonl(UTS);

		if(framelen < 1480)
		{
			rtp_hdr->marker = 1;
			rtp_hdr->seq_no = htons(Rtp_seq_num++);
			
			//printf("marker = %d seq = %d %x ", rtp_hdr->marker, Rtp_seq_num-1, sendbuff[1]);
			memcpy(sendbuff+12, framebuff, framelen);
			sendbuff[framelen + 12] = '\0';
			sendnum = sendto(sockclient, sendbuff, framelen + 12, 0,(struct sockaddr*)&sintcpclientaddr, sizeof(sintcpclientaddr));	
			continue;	
		}

		if(framelen%1480 == 0)//将一帧的数据分成每个报文长1480字节发出去
		{
			spi_frame_num = framelen/1480; //算出一帧数据需要发送几个包出去
		}
		else
		{
			spi_frame_num = framelen/1480 + 1;
		}

		
		//printf("time %d  num %d \n", timestamp_increse, spi_frame_num);
		
		for(i=0; i < spi_frame_num-1; i++)
		{
			rtp_hdr->marker = 0;
			rtp_hdr->seq_no = htons(Rtp_seq_num++);
			memcpy(sendbuff+12, framebuff + (1480*i), 1480);


			//printf("marker = %d seq = %d %x ", rtp_hdr->marker,Rtp_seq_num-1, sendbuff[1]);
			sendbuff[1492] = '\0';
			
			sendnum = sendto(sockclient, sendbuff, 1480+12, 0,(struct sockaddr*)&sintcpclientaddr, sizeof(sintcpclientaddr));	
            //sendto是一个系统函数，将一个RTP报文发出去，指定IP地址发送出去。


			//send(sockclient, sendbuff, 1480+12, 0);		
			//printf("%d \n",sendnum);
		
		}
		//printf("##############################################################\n");
		rtp_hdr->marker = 1;
		rtp_hdr->seq_no = htons(Rtp_seq_num++);
		
		//printf("marker = %d seq = %d %x ", rtp_hdr->marker, Rtp_seq_num-1, sendbuff[1]);
		memcpy(sendbuff+12, framebuff + 1480*i, framelen-(1480*i));
		sendbuff[framelen-(1480*i)+12] = '\0';
		sendnum = sendto(sockclient, sendbuff, framelen-(1480*i)+12, 0,(struct sockaddr*)&sintcpclientaddr, sizeof(sintcpclientaddr));	
		//send(sockclient, sendbuff, framelen-(1480*i), 0);
		//sleep(10);
		//printf("%d \n",sendnum);
	}

	/*************************************************************************************/

	return 1;
}


