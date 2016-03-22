#ifndef _ENCODE_H_
#define _ENCODE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#define IFRAME_MAX_NUM 32

#define BUFFER_SIZE (1.6*1024*1024)  /* 1.6 MB  ������*/

typedef struct _Iframe_list
{
     int flag;
    
     unsigned long iframe_offset;                 /*I ֡�ĵ�ַƫ������*/

     long long  time_stamp;                    /*��¼I֡��ʱ���*/
   
}Iframe_list;




typedef struct _frame_buffer
{
        unsigned char *buffer_start;                 /*��Ƶ�������ʼ��ַ*/
        unsigned long buffer_size;                    /*��Ƶ�����С*/                                       /*10M*/
        unsigned long read_offset;                    /*��Ƶ����Ķ�ƫ��*/
        unsigned long write_offset;                   /*��Ƶ�����дƫ��*/
        unsigned long read_offset_before;     /*��Ƶ�����Ԥ¼ǰn  ��Ķ�ƫ��*/
        unsigned long alarm_end_offset;         /*������Ƶtime ���ڵ����ݵĽ���λ��*/
        
        Iframe_list ikey_list[IFRAME_MAX_NUM];                  /*��Ƶ�����б����I֡��������ʱ��*/
                                                                                                  /*32 �� I ֡��ʱ������ ??*/
        int Ilist_curr;                                          /*����һ��д�뻺���I ֡����*/
        
        //Iframe_list alarm_ikey;                         /*������Ƶ���͵���ʼI ֡*/
        
        time_t alarm_start_time;                  /*������Ƶ���͵���ʼI ֡ʱ��*/
        unsigned int alarm_time_len;           /*������Ƶ�ϴ������ʱ��*/
        unsigned  long alarm_time_end;     /*�Ƿ����ҵ�time ��Ƶ����λ��*/


        //pthread_mutex_t buff_mutex; /*д���廥����   */
        
}Frame_buffer;

typedef struct _alarm_video_para
{
        bool is_all_pre;
	int channel;
	int time_len;
        //char cmd_para0;     //��ʾ�Ƿ���ǰһ���һ�����Ƶ
        //char cmd_para1;     //Ԥ¼������Ƶ��ʱ��
        //unsigned long long alarm_id;     
        unsigned char alarm_id[8];    //����ID
        
}alarm_video_parameter;

typedef struct _send_video_info
{
        int channel;
	int g_send_alarm_video_flag;   	/*�����ͱ���Ԥ¼��Ƶ��־*/

	int g_send_real_video_flag;  	/*������ʵʱ��Ƶ��־*/

	alarm_video_parameter g_alarm_video_para;

        pthread_t g_video_pthreadid;
	pthread_mutex_t alarm_video_mutex;
	pthread_cond_t  g_send_video_cond;
}send_video_info;




class Encode_Class
{
        private:
          //  unsigned char *en_w_buffer;      /*��Ƶ����bufferд��ַ*/
         //   unsigned char *en_r_buffer;       /*��Ƶ����buffer����ַ*/
           int I_curr_tmp;

           // int channel_num;
            struct _frame_buffer *frame_buff;          /**/

            unsigned long r_w_inteval;         /*�ڻ����ж����������ݵļ������*/

            int rw_eq_flag;                             /*��ƫ����  дƫ����ȱ�־    */

            //int Alarm_Iframe;                       /*��Ӧ����time ��I ֡ ����*/

       public:
            pthread_t encode_thread_id;
             int channel_num;

        public:
            Encode_Class();
            ~Encode_Class();

          void  Init_Frame_Buffer(int num);

          int Get_Video_Channel();

            unsigned char *  Creat_Buffer_Pool(unsigned int buffer_size); 

            int  Find_Alarm_Iframe(int time_len, int channel);

            int Send_Start(int time, int is_all, int channel);  /**/
            
            int Get_Data(unsigned char **data, unsigned int count, int *end_flag, int channel);

            void Save_Frame_Data(unsigned char *frame_data, int frame_len, bool is_ikey, int channel);

            void Request_Encode_I_Frame(int Channel,CHANNELTYPE dwStreamType);


};




#endif

