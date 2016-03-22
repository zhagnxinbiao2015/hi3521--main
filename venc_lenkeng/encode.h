#ifndef _ENCODE_H_
#define _ENCODE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#define IFRAME_MAX_NUM 32

#define BUFFER_SIZE (1.6*1024*1024)  /* 1.6 MB  子码流*/

typedef struct _Iframe_list
{
     int flag;
    
     unsigned long iframe_offset;                 /*I 帧的地址偏移索引*/

     long long  time_stamp;                    /*记录I帧的时间戳*/
   
}Iframe_list;




typedef struct _frame_buffer
{
        unsigned char *buffer_start;                 /*视频缓冲的起始地址*/
        unsigned long buffer_size;                    /*视频缓冲大小*/                                       /*10M*/
        unsigned long read_offset;                    /*视频缓冲的读偏移*/
        unsigned long write_offset;                   /*视频缓冲的写偏移*/
        unsigned long read_offset_before;     /*视频缓冲的预录前n  秒的读偏移*/
        unsigned long alarm_end_offset;         /*报警视频time 秒内的数据的结束位置*/
        
        Iframe_list ikey_list[IFRAME_MAX_NUM];                  /*视频缓冲中保存的I帧索引及其时间*/
                                                                                                  /*32 个 I 帧的时间差多少 ??*/
        int Ilist_curr;                                          /*最新一个写入缓冲的I 帧索引*/
        
        //Iframe_list alarm_ikey;                         /*报警视频发送的起始I 帧*/
        
        time_t alarm_start_time;                  /*报警视频发送的起始I 帧时间*/
        unsigned int alarm_time_len;           /*报警视频上传请求的时长*/
        unsigned  long alarm_time_end;     /*是否已找到time 视频结束位置*/


        //pthread_mutex_t buff_mutex; /*写缓冲互斥锁   */
        
}Frame_buffer;

typedef struct _alarm_video_para
{
        bool is_all_pre;
	int channel;
	int time_len;
        //char cmd_para0;     //表示是否是前一半后一半的视频
        //char cmd_para1;     //预录报警视频总时长
        //unsigned long long alarm_id;     
        unsigned char alarm_id[8];    //报警ID
        
}alarm_video_parameter;

typedef struct _send_video_info
{
        int channel;
	int g_send_alarm_video_flag;   	/*请求发送报警预录视频标志*/

	int g_send_real_video_flag;  	/*请求发送实时视频标志*/

	alarm_video_parameter g_alarm_video_para;

        pthread_t g_video_pthreadid;
	pthread_mutex_t alarm_video_mutex;
	pthread_cond_t  g_send_video_cond;
}send_video_info;




class Encode_Class
{
        private:
          //  unsigned char *en_w_buffer;      /*视频缓冲buffer写地址*/
         //   unsigned char *en_r_buffer;       /*视频缓冲buffer读地址*/
           int I_curr_tmp;

           // int channel_num;
            struct _frame_buffer *frame_buff;          /**/

            unsigned long r_w_inteval;         /*在缓冲中读数据与数据的间隔长度*/

            int rw_eq_flag;                             /*读偏移与  写偏移相等标志    */

            //int Alarm_Iframe;                       /*对应报警time 的I 帧 索引*/

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

