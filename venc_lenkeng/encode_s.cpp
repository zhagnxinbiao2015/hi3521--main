

#include "encode.h"
#include "pthread.h"
#include "sys/types.h"
#include "sys/socket.h"
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "alarmserver.h"

#include "log.h"

extern Log_Class log_class;


Encode_Class encode_class; /**/

extern send_video_info *g_send_video_info;


//int  sum_number = 1;
int Udp_Sendto(int sockid, struct sockaddr *addr, unsigned char *data, int data_size)
{
        int len = 0, leave_data;
        int ret = 0;
	unsigned char *p_data = NULL;
	int count = 0;
		
        fd_set fd_write, fd_read, fd_error;
        struct timeval timeout;
        
        if(sockid <= 0)
        {
                //printf("bad socket id! \n");
                return -1;
        }

        if(!data || (data_size <= 0))
        {
        	//printf("bad data! \n");
                return 0;
        }

	p_data = data;
	leave_data = data_size;

	//while(leave_data > 0) //��Ҫ���socket �ж�
	{

	        FD_ZERO( &fd_write );
		FD_ZERO( &fd_read );
		FD_ZERO( &fd_error );
		
		FD_SET(sockid, &fd_write );
		FD_SET(sockid, &fd_read );
		FD_SET(sockid, &fd_error );
		
		timeout.tv_sec = 1;
		timeout.tv_usec = 500000;
		
	        ret = select((sockid + 1), &fd_read, &fd_write, &fd_error, &timeout);
	        if(ret == 0)
	        {
	                printf("socket do not  write!\n");
	                return -1;
	        }
		else if(ret < 0)
		{
			printf("select error = %s \n", strerror(errno));
			return -1;
		}

		if(FD_ISSET(sockid, &fd_error))
		{
			printf("select sock error = %s \n", strerror(errno));
		}

		if(FD_ISSET(sockid, &fd_read))
		{
			char p_tmp[32];
			ret = recv(sockid, p_tmp, 32, 0);
			if(ret < 0)
			{
				printf("not have recv data ! < 0\n");
				return -2;
			}
			else if(ret == 0)  //����socket���� ���� 
			{
				printf(" recv data is empty! may be socket break\n"); 
				return -2;
			}
			else printf("have recv data!\n");
		}
		
		if(FD_ISSET(sockid, &fd_write))
		{	
			len = send(sockid, data, data_size, 0);
		        //len = sendto(sockid, data, data_size, 0, addr, sizeof(struct sockaddr_in));
		        if(len < 0)
		        {
		                printf("send data failed!error = %s\n", strerror(errno));
		                return -2;
		        }
			else if(len == 0)
				printf("send 0 data!\n");

		//	if(len != data_size)
			//	printf("$$$$$$$$$$$$$$$$$$$$$$$$\n");
				
			//leave_data = 0;
		}
		else
		{
			printf("select ok, but can not write!\n");
		}
		leave_data = leave_data - len;
		p_data += len;

	}

	
	//printf("%d \n", sum_number++);
          return len;
}

#ifdef test
FILE *fp;
int num = 0;
void save_alarm_video()
{
	char path[32];
	sprintf(path, "/home/local_sdk/%d", ++num);
	fp = fopen(path, "a+");
}

#endif
/**************************************
�����ͱ�����Ƶ:

�������: time �Ƿ���time �����Ƶ
                          is_all �Ƿ���ǰtime�����Ƶ����ǰһ���һ��time����Ƶ
                          sockid ����Ƶ������socket id��
                          
***************************************/     
void  Send_Alarm_Video(int time, bool is_all, int sockid, struct sockaddr *addr, int chn)
{
        unsigned int count = 1400; /*��ֵÿ��getdata �����ȡ����Ƶ���ݴ�С*/
        unsigned char *data =  NULL;            /*ָ��getdata ����Ƶ����ָ��,   ��ûcopy ����*/
	int channel = 0;
	int ret;
        int len;
        unsigned long sum_len = 0;
        int end = 0;
        int fd = sockid;
	int chn_tmp = chn;

        /*�����һ��I ֡ */
        encode_class.Request_Encode_I_Frame(0, SDK_CHL_2END_T);   
	usleep(1000*280);
	
        encode_class.Send_Start(time, is_all, chn_tmp); /**/

	if(chn < 0 || chn > 3)
		return ;

	
	#ifdef test
   	save_alarm_video();
	#endif
        printf("read start %d channel!\n", chn);
	log_class.Write_Debug_Log(0, "read start !time = %d\n",  time);
        while(1) /*��ȡtime ����Ƶ���˳�while*/
        {
               len = encode_class.Get_Data(&data, count, &end, chn_tmp);  /* һ������ȡ��Ƶ����count���ֽ�*/  /* ���ǰ�֡����ȡ����*/
                                                                                                         /*end �Ƿ����time ���ڵ���Ƶ����*/
                                                                                                         /*����data ���ݵ�ʵ�ʳ���*/
													 /*���get����һֱ�ǿյĻ�  ??????*/
                sum_len +=  len;   
																										 
                ret = Udp_Sendto(fd, addr, data, len);
		if(ret < 0)  /*socket �Ͽ� ����*/
		{
			log_class.Write_Debug_Log(0, "send socket error!\n");
			break;
		}
		#ifdef test
		fwrite(data, len, 1, fp);
                fflush(fp);
		#endif
                if(end == 1 ) /*�ѻ�ȡ��Ԥ¼time ���ڵ���Ƶ����*/
                {
                	#ifdef test
			fclose(fp);
			#endif
                        break;
                }
		
                if(g_send_video_info[channel].g_send_real_video_flag == 2)/*�˳��ϴ�ʵʱ��Ƶ*/
                {
			break;
		}
		else if(g_send_video_info[channel].g_send_real_video_flag == 3)/*ˢ���ϴ�ʵʱ��Ƶ*/
		{
			chn_tmp = g_send_video_info[channel].channel;
			encode_class.Request_Encode_I_Frame(chn_tmp, SDK_CHL_2END_T);   
			usleep(1000*280);
			encode_class.Send_Start(time, is_all, chn_tmp); /**/
			g_send_video_info[channel].g_send_real_video_flag = 1;
			
		}
		if(get_login_status() != REGISTER_ALARM_OK)  /*�豸�������������״̬*/
		{
			//log_class.Write_Debug_Log(0, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$!\n");
			break;
		}
        }
		
        printf("read over ! %ld\n", sum_len);
	log_class.Write_Debug_Log(0, "read over ! %ld\n", sum_len);
	
        return ;
}






















