#include "encode.h"

#include "log.h"

extern Log_Class log_class;


Encode_Class::Encode_Class()
{
;
}

Encode_Class::~Encode_Class()
{
    ;
}

int Encode_Class::Get_Video_Channel()
{
	return channel_num;
}

void  Encode_Class::Init_Frame_Buffer(int num)
{
	int index = 0;
	int i_index = 0;

	if(num < 0)
		return ;

	channel_num = num;
	
	frame_buff = (struct _frame_buffer  *)malloc(channel_num * sizeof(Frame_buffer));

	for(index = 0; index < channel_num; index++)
	{
		frame_buff[index].buffer_size = BUFFER_SIZE;
		//printf("buffer size = %ld \n", frame_buff[index].buffer_size);

		frame_buff[index].buffer_start = Creat_Buffer_Pool(frame_buff[index].buffer_size);
		if(frame_buff[index].buffer_start == NULL)
		{
			printf("malloc video buffer failed!\n");
		}

		frame_buff[index].alarm_end_offset = 0;
		frame_buff[index].alarm_start_time = 0;
		frame_buff[index].read_offset = 0;
		frame_buff[index].read_offset_before = 0;
		frame_buff[index].write_offset = 0;


		frame_buff[index].Ilist_curr = 0;

		frame_buff[index].alarm_time_len = 0;
		frame_buff[index].alarm_start_time = 0;

		for(i_index = 0; i_index < IFRAME_MAX_NUM; i_index++)
		{
			memset(&(frame_buff[index].ikey_list[i_index]), 0, sizeof(Iframe_list));
		}
	}
	
}
/************************************************
申请视频数据的缓冲，
输入参数:  buff_size   需要申请的buffsize大小的内存
*************************************************/
unsigned char *  Encode_Class::Creat_Buffer_Pool(unsigned int buff_size)
{
	unsigned char *buffer = NULL;

	buffer= (unsigned char *)malloc(buff_size);
	if(buffer == NULL)
	{
		printf("malloc memory fail\n");
		return NULL;
	}

	memset(buffer,0,buff_size);	

	return buffer;
}

/*************************************************
在I 帧索引数组中查找前time len 时长视频数据的I 帧
**************************************************/
int  Encode_Class::Find_Alarm_Iframe(int time_len, int channel)
{
	int Ifram_interval;
	
	int index =  I_curr_tmp;

	int compare_num = 1;
	while(compare_num < IFRAME_MAX_NUM)
	{
		if(index == 0)
		{	
			index = IFRAME_MAX_NUM - 1;
		}
		else
		{	
			index = index - 1 ;
		}
		
		
#if 0
		if(compare_num == time_len)
			return index;
		if(frame_buff.ikey_list[index].flag == 0)
			continue;
#endif
		if(frame_buff[channel].ikey_list[index].time_stamp <= 0)
		{	
			if(index ==  IFRAME_MAX_NUM - 1)
				return 0;
			else return (index + 1);
		}

		Ifram_interval = frame_buff[channel].ikey_list[I_curr_tmp].time_stamp - frame_buff[channel].ikey_list[index].time_stamp;
		if(Ifram_interval >= (time_len)) /*实际情况总是打不到time len 秒视频*/
		{	
			return index;
		}

		compare_num++;
	}
	
	//return -1;
	return index;
	

}
#if 0
	int Iframe_inval;   /*当前I帧 与最早I帧的时间间隔*/
	
	if(frame_buff.Ilist_num == (IFRAME_MAX_NUM - 1))
	{
		Iframe_inval = frame_buff.ikey_list[IFRAME_MAX_NUM - 1].time_stamp - frame_buff.ikey_list[0] .time_stamp;
	}
	else
	{
		Iframe_inval = frame_buff.ikey_list[frame_buff.Ilist_num].time_stamp - frame_buff.ikey_list[frame_buff.Ilist_num + 1] .time_stamp;
	}

	if(Iframe_inval < time_len)
		return -1;
#endif
	

void Encode_Class::Request_Encode_I_Frame(int Channel,CHANNELTYPE dwStreamType)
{

	LOCALSDK_FORCE_I_FRAME IFrame;
	IFrame.Channel = Channel;
	IFrame.dwStreamType = dwStreamType;
	LOCALSDK_ForceIFrame(&IFrame);

}


/**************************************
输入参数: time 是发送time 秒的视频
                          is_all 是发送前time秒的视频或发送前一半后一半time的视频
                          time 为0  就是请求看实时视频
				time 是否有最小值限制
返回值: 
***************************************/
 int  Encode_Class::Send_Start(int time_len, int is_all, int channel) /*读最近的I 帧，获取前time 秒的I 帧， 或前一半time秒的I 帧*/
{
	int alarm_iframe_index;
	struct tm curr_i_time;
	
	I_curr_tmp = frame_buff[channel].Ilist_curr;
	
	gmtime_r((time_t *)&frame_buff[channel].ikey_list[I_curr_tmp].time_stamp, &curr_i_time);
	printf("hour = %d minute = %d second = %d  \n", curr_i_time.tm_hour + 8, curr_i_time.tm_min, curr_i_time.tm_sec);
	log_class.Write_Debug_Log(0, "%02dh/%02dm/%02ds  ", curr_i_time.tm_hour + 8, curr_i_time.tm_min, curr_i_time.tm_sec);

	if(time_len == 0) /*请求浏览实时视频 ???*/
	{
		frame_buff[channel].alarm_time_len = 0;
		frame_buff[channel].alarm_time_end = 0;
		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[I_curr_tmp].iframe_offset;
		printf("read offset = %ld \n", frame_buff[channel].read_offset_before);
		return 0;
	}

	//printf("curr i time = %lld   ", frame_buff[channel].ikey_list[I_curr_tmp].time_stamp);
	if(is_all)     /*表示需要读取前time 秒的视频，可以以当前I帧偏移作为time 秒结束位置*/
	{
		
		frame_buff[channel].alarm_time_len = time_len;   
		frame_buff[channel].alarm_time_end = 1;  	
		frame_buff[channel].alarm_end_offset = frame_buff[channel].ikey_list[I_curr_tmp].iframe_offset; /*报警前time 秒的视频结束位置，当前I 帧*/

		alarm_iframe_index = Find_Alarm_Iframe(time_len, channel);
		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[alarm_iframe_index].iframe_offset;
	}
	else            /*表示需要读取前一半time秒的视频，后一半time 秒的视频，
				需要在保存录像的同时确定time 秒视频的结束位置				*/
	{
		frame_buff[channel].alarm_time_len = time_len;                    /*记录视频时长 time秒*/
		frame_buff[channel].alarm_time_end = 0;  	                             /*表示time 秒的视频结束位置没确定*/
		
		/****************************
		通过I 帧索引数组找到报警视频的起始I 帧位置
		如果没有找到呢 ?说明I 帧列表中缓冲中没有前time 秒的视频
		*****************************/
		alarm_iframe_index = Find_Alarm_Iframe(time_len/2, channel);
		
		frame_buff[channel].alarm_start_time =  frame_buff[channel].ikey_list[alarm_iframe_index].time_stamp;     /*报警视频的起始I 帧时间戳*/

		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[alarm_iframe_index].iframe_offset;  /*读报警视频time 的起始位置*/
		frame_buff[channel].alarm_end_offset = 0;
	}

	//printf(" alarm time = %lld \n", frame_buff[channel].ikey_list[alarm_iframe_index].time_stamp);
	printf("I_ccur = %d, alarm_iframe_index = %d ,read_offset = %ld   end_offset = %ld\n", frame_buff[channel].Ilist_curr, alarm_iframe_index, frame_buff[channel].read_offset_before, frame_buff[channel].alarm_end_offset);

	return 0;
}


/*******************************************************
输入参数:count  最大获取的数据字节数

从这个read_offset_before 开始读取报警视频time 秒 的视频

返回参数:data  
				end_flag : true为time秒视频读取完毕，false 为time秒视频未读取完毕


需要控制读缓冲数据  与  写 缓冲数据的同步    怎么控制?????

实时是否需要判断读和写的偏移是否相等，就怕万一读的与写的速度不一样

返回值: 
********************************************************/
//int tmp_flag = 0;
int count1 = 0;
int  Encode_Class::Get_Data(unsigned char **data, unsigned int count, int *end_flag, int channel)
{
	unsigned int len;

	 unsigned long write_offset_tmp = frame_buff[channel].write_offset; 
	 
	//printf("get data ~\n");
	if(frame_buff[channel].read_offset_before >= frame_buff[channel].buffer_size)
	{	
		frame_buff[channel].read_offset_before = 0;
	}
	
	*data = frame_buff[channel].buffer_start + frame_buff[channel].read_offset_before;   /*读取数据指向的地址 ，可以不需要copy 过去*/
	
	
	if(frame_buff[channel].alarm_time_end == 0)/*---未找到视频的结束位置---*/
	 {						  /*读取count 个字节*/
		*end_flag = 0; 
		if (write_offset_tmp == frame_buff[channel].read_offset_before)//读等于写，等待写数据 返回0 个字节; 
		{ 
			//printf("waiting write frame data!%d\n",count1++); 
			usleep(1000*20);
			return 0; 
		} 
		
		if(write_offset_tmp  > frame_buff[channel].read_offset_before)
		{	
			len = write_offset_tmp - frame_buff[channel].read_offset_before;
			if(len > count)
			{
				frame_buff[channel].read_offset_before += count;
				return count;
			}
			else
			{
				frame_buff[channel].read_offset_before += len;		/* 此时读偏移追上写偏移 */
	
				return len;
			}
		}
		else 
		{
			len = frame_buff[channel].buffer_size - frame_buff[channel].read_offset_before;
			if(len > count)                       
			{
				frame_buff[channel].read_offset_before += count;  
				return count;
			}
			else
			{
				frame_buff[channel].read_offset_before += len;           
				return len;
			}
		}
		
		return 0;
	 }


	/*	-------找到time 秒视频的结束位置------
	比较报警视频此时读的视频数据位置  和 报警视频数据的结束位置
	没有等于的情况吧， 等于就是获取time 秒内报警视频结束
	写数据会追上读数据吗 ??
	*/
	//usleep(1000*2);
	if(frame_buff[channel].read_offset_before > frame_buff[channel].alarm_end_offset)  /*报警视频读偏移 大于 报警视频结束偏移*/
	{
#if  0
		if(write_offset_tmp >= frame_buff.read_offset_before   || (write_offset_tmp <= frame_buff.alarm_end_offset))
		{ 
			?;//写数据的追上了读数据   如何处理呢 
		} 
#endif
		*end_flag = 0; /**/ 
		 len = frame_buff[channel].buffer_size - frame_buff[channel].read_offset_before;
		if(len > count)
		{
			frame_buff[channel].read_offset_before += count;
			return count;
		}
		else
		{
			frame_buff[channel].read_offset_before = 0; 	/*读视频到缓冲的顶端，再从0 开始读*/
			return len;
		}
		
	}
	else if(frame_buff[channel].read_offset_before < frame_buff[channel].alarm_end_offset)/*报警视频读偏移 小于 报警视频结束偏移*/
	{
		#if 0
		/**/
		if(write_offset_tmp >= frame_buff.read_offset_before && write_offset_tmp <= frame_buff.alarm_end_offset)
		{
			;//写数据的追上了读数据  如何处理呢
		}
		#endif
		len = frame_buff[channel].alarm_end_offset - frame_buff[channel].read_offset_before;
		if(len > count)
		{
			*end_flag = 0;  /*报警time 秒视频读未完*/
	
			frame_buff[channel].read_offset_before += count;
			return count;
		}
		else
		{
			*end_flag = 1;  /*报警time 秒视频读完毕*/

			len = frame_buff[channel].alarm_end_offset - frame_buff[channel].read_offset_before;

			/*已获取完报警时长的视频数据*/
			frame_buff[channel].alarm_time_len = 0;
			
			return len;
		}
	}
	else
	{
		*end_flag = 1; 
		frame_buff[channel].alarm_time_len = 0;
		return 0;
	}
		

	return len;
}

#if 0
/************************************************
在写I 帧的时候，检验一下是否会覆盖其他的I 帧偏移
flag = 0 为 无效 I 帧偏移
flag = 1 为有效I 帧偏移
*************************************************/
void Encode_Class::Check_Iframe_valid()
{
	int index = 0;
	int ilist_pre;
	
	if(frame_buff.Ilist_curr == 0)
		ilist_pre = IFRAME_MAX_NUM - 1;
	else
		ilist_pre = frame_buff.Ilist_curr  - 1;

	for(index=0; index < IFRAME_MAX_NUM -1; index ++)
	{
		if((index == frame_buff.Ilist_curr) || (index == ilist_pre))
			continue;

		if(frame_buff.ikey_list[frame_buff.Ilist_curr].iframe_offset > frame_buff.ikey_list[ilist_pre].iframe_offset)
		{
			if((frame_buff.ikey_list[index].iframe_offset >= frame_buff.ikey_list[ilist_pre].iframe_offset) &&
				(frame_buff.ikey_list[index].iframe_offset =< frame_buff.ikey_list[frame_buff.Ilist_curr].iframe_offset) )
			{
				frame_buff.ikey_list[index].flag = 0;
			}
		}
		else if(frame_buff.ikey_list[frame_buff.Ilist_curr].iframe_offset < frame_buff.ikey_list[ilist_pre].iframe_offset)
		{
			if((frame_buff.ikey_list[index].iframe_offset >= frame_buff.ikey_list[ilist_pre].iframe_offset) ||
				(frame_buff.ikey_list[index].iframe_offset <= frame_buff.ikey_list[frame_buff.Ilist_curr].iframe_offset) )
			{
				frame_buff.ikey_list[index].flag = 0;
			}
		}
		
	}
}
#endif

/*************************************************
输入参数:  frame_data  帧视频数据起始地址
                            frame_len    帧视频数据长度
                            isi_key        是否是I 帧
**************************************************/
void  Encode_Class::Save_Frame_Data(unsigned char *frame_data, int frame_len, bool is_ikey, int channel)
{
	int i_num;

	if(frame_buff[channel].write_offset >= frame_buff[channel].buffer_size)
	{	
		frame_buff[channel].write_offset = 0;
	}
	//printf("frame len = %d \n", frame_len);
	/*是否 I 帧视频数据， 记录I帧时间及缓冲中偏移地址*/
	if(is_ikey)                  
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

		frame_buff[channel].ikey_list[i_num].iframe_offset = frame_buff[channel].write_offset;   /*记录此I 帧的视频数据偏移量*/
		frame_buff[channel].ikey_list[i_num].time_stamp = time(NULL);   			 /*记录此I 帧的时间还是记录??*/
		//if(channel == 1)
			//printf(" i_curr = %d time = %lld  write offset = %ld \n", i_num,frame_buff[channel].ikey_list[i_num].time_stamp, frame_buff[channel].ikey_list[i_num].iframe_offset);
		
		//frame_buff.ikey_list[i_num].flag = 1;
		
		//Check_Iframe_valid(); //检查上一个I 帧与 这个I 帧之间是否会覆盖已有的I 帧
		#if 1
		if((frame_buff[channel].alarm_time_len != 0) &&               /*是否有报警time 视频时长*/
			(frame_buff[channel].alarm_time_end == 0) &&		/*是否已找到time 视频结束位置*/
			((frame_buff[channel].ikey_list[i_num].time_stamp - frame_buff[channel].alarm_start_time)  >= frame_buff[channel].alarm_time_len))  /*是否有报警视频上传 time 限制,判断报警视频数据的结束位置*/
		{	/*大于等于报警视频所需的时长，*/
			/*记录报警视频time 秒内结束位置*/
			frame_buff[channel].alarm_end_offset = frame_buff[channel].write_offset;

			frame_buff[channel].alarm_time_end = 1;		/*已找到报警time 秒视频结束位置*/

			//printf("find over end offset ! read offset = %ld ,  end offset = %ld\n", frame_buff.alarm_end_offset,frame_buff.read_offset_before);
		}
		#endif

  
		//printf(" %ld = %d %d %d %d  \n", frame_buff.write_offset, frame_data[0],frame_data[1],frame_data[2],frame_data[3]);
	}

#if 0
	//有预录报警视频上传 
 	//如果缓冲小             检测写是否能追上读 
 	if((frame_buff.alarm_time_len != 0) /*&& (frame_buff.alarm_time_end == 0)*/) 
 	{
 		unsigned long w_r_len;
		unsigned long alarm_read_offset = frame_buff.read_offset_before;
 		
		if(frame_buff.write_offset > alarm_read_offset)
		{
			w_r_len = frame_buff.buffer_size - frame_buff.write_offset + alarm_read_offset;
		}
		else if(frame_buff.write_offset < alarm_read_offset)
		{
			w_r_len = alarm_read_offset -  frame_buff.write_offset ;
		} 
		else  if(rw_eq_flag != 1)/*读  写 偏移相等,    */ 
		{  
			w_r_len = frame_buff.buffer_size;
			;//r_w_len;					
		}   
		 
		if(frame_len >=  w_r_len)    /*说明此刻写数据后要超过或等于读数据的偏移，   */   
		{    
	 		rw_eq_flag = 1;  // 写追上读偏移， 怎样处理     ???    是 先拷贝一部分需要读的视频数据  ??     
		} 
		else 
		{
			rw_eq_flag = 0;  
		}
		 
		 
	}

#endif

	//unsigned int write_offset_temp = frame_buff.write_offset;

	/*将此帧视频数据保存到缓冲中*/


	/*比较缓冲中剩余的大小和此视频数据的大小*/
	
	int leave_len = frame_buff[channel].buffer_size - frame_buff[channel].write_offset;
	
	if(leave_len > frame_len)
	{
		memcpy(frame_buff[channel].buffer_start + frame_buff[channel].write_offset, frame_data, frame_len);
		
		frame_buff[channel].write_offset += frame_len;			/*下一次写缓冲的偏移*/
		
	}
	else if(leave_len == frame_len)
	{
		memcpy(frame_buff[channel].buffer_start + frame_buff[channel].write_offset, frame_data, frame_len);
		frame_buff[channel].write_offset = 0;
	}
	else
	{
		memcpy(frame_buff[channel].buffer_start + frame_buff[channel].write_offset, frame_data, leave_len);

		memcpy(frame_buff[channel].buffer_start, frame_data + leave_len, frame_len - leave_len);

		frame_buff[channel].write_offset = frame_len - leave_len;/*下一次写缓冲的偏移*/
	}
	
}



