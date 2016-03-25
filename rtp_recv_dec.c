/******************************************************************************
  A simple program of Hisilicon HI3531 video decode implementation.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-12 Created
******************************************************************************/
//  ���еĺ����ͱ�������ʾ����ߡ�
//���е����ӵ������Ŀ���ļ�����ʾ���ұߡ����ڵ������Ŀ����ĺ�������һ���ļ����档

//���ǽ���εĳ���






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
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;   //��VIDEO_ENCODING_MODE_PAL��ֵ����ǰ���ȫ�ֱ���gs_enNorm��
HI_U32 gs_u32ViFrmRate = 0;
SAMPLE_VDEC_SENDPARAM_S gs_SendParam[SAMPLE_MAX_VDEC_CHN_CNT];
HI_S32 gs_s32Cnt;

    


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
    stAttr.u32Priority = 1;//此处必须大于0
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





/******************************************************************************
* function : vdec process
*            vo is sd : vdec -> vo
*            vo is hd : vdec -> vpss -> vo
******************************************************************************/
HI_S32 Init_MultiSock_VDEC_Process(PIC_SIZE_E enPicSize, PAYLOAD_TYPE_E enType, HI_S32 s32Cnt, VO_DEV VoDev)//Ϊ������һ��Ӳ����ʼ��
{
    VDEC_CHN VdChn;// ����һ������ͨ���ı���
    HI_S32 s32Ret;   //����һ�����صı�����������ִ������ȷ���Ǵ���
    SIZE_S stSize;   //����һ����С�ı���
    VB_CONF_S stVbConf;   //��һ���ṹ��������ṹ����������ĳ�Ա������˵���������ı�����
    HI_S32 i;   //����һ������������
    VPSS_GRP VpssGrp;  
    VIDEO_MODE_E enVdecMode;   //����һ������ģʽ����
    HI_CHAR ch;  //����һ��ͨ��������

    VO_CHN VoChn;  //����һ�����ͨ������
    VO_PUB_ATTR_S stVoPubAttr;     //����һ���ṹ��
    SAMPLE_VO_MODE_E enVoMode;     //����һ�����ģʽ�ı���
    HI_U32 u32WndNum, u32BlkSize;   //��������������Ƕ���һ����Ĵ�С

    HI_BOOL bVoHd; // through Vpss or not. if vo is SD, needn't through vpss     //����һ����������
 
    /******************************************
     step 1: init varaible.
    ******************************************/
    gs_u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL == gs_enNorm)?25:30;   //��ȡ��Ƶ����ʽ����25֡����30֡��ͨ��ȫ�ֱ���gs_enNorm���˽⵽��PAL����NTSC����֡��25����30��������������������������棬��ô��ֵ��ǰ��ı�������25������Ǽ٣���ô�͸��������30 ��
    
    if (s32Cnt > SAMPLE_MAX_VDEC_CHN_CNT || s32Cnt <= 0)   //�ж��������ֵ�Ƿ񳬹� SAMPLE_MAX_VDEC_CHN_CNT���ж�����Ľ���ͨ��ֵ�Ƿ񳬹����ֵ������С��0��Ҳ����û�У���ô���ش���s32Cnt�������Init_MultiSock_VDEC_Process�����е�һ��������
    {
        SAMPLE_PRT("Vdec count %d err, should be in [%d,%d]. \n", s32Cnt, 1, SAMPLE_MAX_VDEC_CHN_CNT);  //�����һ����ж���С��0�Ļ��ߴ��ڽ���ͨ�����ֵ����ô����ʾ������Ϣ����������������
        
        return HI_FAILURE;  //����������
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enPicSize, &stSize);  //��������Ĺ����ǵõ��ض������ͼƬ�Ĵ�С��������ͬ�ֱ��ʵĳ������ǲ�һ����
    if (HI_SUCCESS !=s32Ret)  //������������õ�һ������ֵ����s32ret  ���Ǽ٣���ִ���������ʾ������Ϣ��������һ������ֵ����������������Ǹ�����ĺ�����
    {
        SAMPLE_PRT("get picture size failed!\n");   //��ʾ���󱨸档
        return HI_FAILURE;
    }
    if (704 == stSize.u32Width)  // 704����Ƶ��ʽΪD1�Ŀ���ֵ����ͷ�ļ�������Բ鵽��ͷ�ļ�оƬ���ҵ�SDK�����ṩ�ġ�
    {
        stSize.u32Width = 720;
    }
    else if (352 == stSize.u32Width)  //352�ǵ���CIF�ĸ�ʽ��
    {
        stSize.u32Width = 360;
    }
    else if (176 == stSize.u32Width)  //176��QCIF���൱�ֱܷ��ʺܵ͵�
    {
        stSize.u32Width = 180;
    }
    
    // through Vpss or not. if vo is SD, needn't through vpss
    if (SAMPLE_VO_DEV_DHD0 != VoDev )   //���ݱ���VoDev ,Vodev��������MAIN�������ֹ���ֵ�ġ��ж�����豸�Ǹ���Ļ��Ǳ���ģ�
    {
        bVoHd = HI_FALSE; //����жϺ�Vodev������SAMPLE_VO_DEV_DHD0 ��Ҳ���Ǹ����ʽ����ô��ֵ��
    }
    else
    {
        bVoHd = HI_TRUE;
    }
    /******************************************
     step 2: mpp system init.
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));

    //u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(gs_enNorm,\
    //           PIC_D1, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

	
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
  
    /******************************************
     step 3: start vpss, if ov is hd.
    ******************************************/  
#if 0
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

   
      
     stVoPubAttr.enIntfSync = VO_OUTPUT_720P50;

    
    stVoPubAttr.enIntfType = VO_INTF_VGA;
    stVoPubAttr.u32BgColor = 0x000000ff;
    stVoPubAttr.bDoubleFrame = HI_FALSE;

 
    s32Ret = SAMPLE_COMM_VO_StartDevLayer(VoDev, &stVoPubAttr, gs_u32ViFrmRate);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartDevLayer failed!\n");
        goto END_1;
    }
    
    s32Ret = SAMPLE_COMM_VO_StartChn(VoDev, &stVoPubAttr, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_2;
    }

 
#if 0
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
       
#if 0     
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


/*����RTP ֡ͷ*/
int Parse_RtpHeader(unsigned char *buff, unsigned int len, unsigned int *pre_pts, int *payloadoffset)
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
		printf(" %d -", buff[i]);
	}
	printf("\n");


	printf("version = %d , payload = %d ,marker = %d, uts = %ld\n", rtp_hdr->version, rtp_hdr->payload, rtp_hdr->marker, ntohl(rtp_hdr->timestamp));
#endif

	//printf("#############seq = %d\n", ntohs(rtp_hdr->seq_no));
	//96��h264 ��Ƶ��ʽ�����ͣ���Ƶ�ĸ�������ֵҲ�Ǹ�����Ƶ�������ֵ�
	if((2 != rtp_hdr->version)  ||  (96 != rtp_hdr->payload) /*|| (98 != rtp_hdr->payload) */)
	{
		//rtp �汾�������RTP �������Ͳ���96(H264)  ,  ��
		//�Ƿ���Ҫ���ж���ssrc ����ϢԴ
		//����Ҫ�ж���չ���ݳ����Լ�csrc len �� 
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
		//����һ֡��Ƶ�������
	
		return rtp_pac_end;
	}
	else
	{
		//����Ҫ���յ�һ֡�����е����һ��rtp ��
		
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

	//�����鲥����socket
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

    //�����鲥��
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

	//ѭ������H264 RTP��Ƶ�����鲥����
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
		rtp_marker = Parse_RtpHeader(buff, n, &nowUTS, &payloadoffset);

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
		//�����յ���һ֡��Ƶ���ݽ��н���
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
	len = recvfrom(sockid, buff, 1500, 0, (struct sockaddr *)&servsockaddr, &addr_len);
	printf("len = %d \n", len);
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
     servaddr.sin_port = htons(port);  ///�������˿�
     servaddr.sin_addr.s_addr = inet_addr(addr);  ///������ip
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

int main(int argc, char* argv[] )
{
	int sockid;  //SOCKET  ID ����      
	unsigned char buff[1500];   //���նˣ�MTU��̫����ͨѶ�����һ����1500�ֽڣ�
	int len;   
	int payloadoffset;
	unsigned int preUTS = 0,nowUTS = 0;
	rtp_marker_type rtp_marker;
	unsigned char framebuff[311040];
	unsigned int framelen = 0;
	VDEC_STREAM_S stStream;

	char *cmd = "ifconfig eth0 192.168.1.20";
	system(cmd);     //����������䣬���ǰ�IP��ַָ��Ϊ192.168.1.20  ����ͨ���Ļ���ָ��IP��ַ����LINUX�ű�����ָ����
	
	//   char *cmd = "route add -net 224.0.0.0 netmask 224.0.0.0 eth0";  //����һ��·��
	
	// system(cmd); //ִ�����ϵ������һ����һ��ϵͳ����

	gs_s32Cnt = 1;//����һ��ȫ�ֱ������������ʱ����һ����Ļ��

	Init_MultiSock_VDEC_Process(PIC_D1, PT_H264, gs_s32Cnt, SAMPLE_VO_DEV_DHD0);//Ϊ������׼���ĺ�������ʼ��Ӳ��������������Ǻ����Ĳ��������������ִ�е�ʱ�� ��������ı���ִ�С���������ı����Ѿ��и�ֵ�
	struct sockaddr_in servsockaddr;

	//sockid = IntiRtpRecSock(&servsockaddr);//��ʼ���鲥SOCKET,���ղ������鲥��RTP�����ġ������ں���������д����

	//struct sockaddr_in servsockaddr;

	//sockid = InitRtpTcpSock(8888, "192.168.1.10", &servsockaddr); //����TCP socket

	sockid = InitRtpUdpSock(8888);      //����UDPsocket,  ���ڽ�������UPD��socket ,�����TCP�����ñ�ĺ�����

	// ���������LINUX ϵͳAPI ���� socket�� SOCKET�ţ�����������SOCKET����ͨѶ���Ͳ�����IP��ַ��ͨѶ�ġ�
	
	socklen_t addr_len = sizeof(servsockaddr);   // servsockaddr����һ��ϵͳ�ṹ�壬������IPV4��IPV6Э�����ͣ����ж˿ں͵�ַ��

//sizeof��C��������������������ṹ��ĳ��ȡ�

	printf("sockid %d\n",sockid);
	while(1)
	{	memset(buff, 0, 1500);   //memset��ϵͳ���������ڴ�������á����ڱ��˵ĳ�������Ҳ��������ơ�buff���������涨��ı�����0�ǰ��ڴ�����Ķ���Ϊ0,1500��BUFF�Ĵ�С��
		len = recvfrom(sockid, buff, 1500, 0, (struct sockaddr *)&servsockaddr, &addr_len);  //recvfrom��SOCKID��SOCKET��ȡ���ݱ��ġ���ȡ�ı��ķ���buff���档���յ��ı��ĳ��ȸ���len��
		//if(len > 0)
			//printf("len = %d \n", len);

	    //continue;
		//len = recv(sockid, buff, 1500, 0);
		
		//continue;
		//printf("recv success \n");
		rtp_marker = Parse_RtpHeader(buff, len, &nowUTS, &payloadoffset);  //buff���յ��ı��ģ���RTP��ʽ���н��롣�����Ƿ���һ֡���ݵı��ģ����ظ�RTPMAKER ,Ŀ�����ж��Ƿ�һ֡����ģ�����ǣ�����������������벥�š�
		if(rtp_marker == rtp_pac_end) //һ֡������Ͻ�֮ǰ���յ�������װ��һ֡����Ƶ����
		{
			memcpy(framebuff + framelen, buff + payloadoffset, len - payloadoffset);//������յ���֡�Ľ�β���ͽ����ڴ濽������BUFF������֡����FRAMEBUFF���棬�����������FRAMEBUFF����ȡ���������
			framelen = framelen + len - payloadoffset;  //���Եõ�һ֡�ĳ��ȣ��������������Ҫ�õ���
		}
		else if(rtp_marker == rtp_pac_inter)//һ֡δ������ϣ�����continue ѭ��������һ��RTP����
		{
			memcpy(framebuff + framelen, buff + payloadoffset, len - payloadoffset);
			framelen = framelen + len - payloadoffset;
			
			continue;  //continue�Ǻ�while��Ӧ�ġ�����˵���������֡�Ľ�β���ͼ���������һ�����ġ�
		}
		else
		{
			continue;
		}
		printf("time %d len %d\n", nowUTS - preUTS, framelen);
		u64PTS = u64PTS + (nowUTS - preUTS); //ʱ�������
		stStream.u64PTS  = u64PTS;
    	stStream.pu8Addr = framebuff; //һ֡��Ƶ�ĵ�ַ
    	stStream.u32Len  = framelen; //һ֡��Ƶ�ĳ���

		HI_MPI_VDEC_SendStream(0, &stStream, HI_IO_BLOCK);   //HI_MPI_VDEC_SendStream����SDK��������ݡ�������оƬ�����ṩ�ġ�
	//ststream��һ���ṹ�壬����ṹ�����������������stStream.u64PTS��SDK����ָ����ʱ����������pstStream.pu8Addr ��SDK����ָ����֡��ַ�ı�����stStream.u32Len��SDK��ָ����֡�ĳ��ȡ�
		framelen = 0;  //Ҫ��ʼ������һ֡���ݣ�����Ҫ��֡�ĳ������㡣

		preUTS = nowUTS; //�����ڵ�ʱ���ʱ������һ֡ʱ���ˡ�
		}
	
	return HI_SUCCESS; //return  ִָ�е���������ȫ���ͷ��ˡ�
}


 
//#if 0   //if 0 ������ĳ�����ǲ�ִ�еģ����ø���ô���'' // '' ,����Ҫ��''end if  ''����Ϊ���������ξ�ȡ���ˡ�
//duuiid
//#endif
//#ifdef __cplusplus
//#if __cplusplus
//}
//#endif
//#endif /* End of #ifdef __cplusplus */
