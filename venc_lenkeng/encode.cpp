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
������Ƶ���ݵĻ��壬
�������:  buff_size   ��Ҫ�����buffsize��С���ڴ�
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
��I ֡���������в���ǰtime len ʱ����Ƶ���ݵ�I ֡
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
		if(Ifram_interval >= (time_len)) /*ʵ��������Ǵ򲻵�time len ����Ƶ*/
		{	
			return index;
		}

		compare_num++;
	}
	
	//return -1;
	return index;
	

}
#if 0
	int Iframe_inval;   /*��ǰI֡ ������I֡��ʱ����*/
	
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
�������: time �Ƿ���time �����Ƶ
                          is_all �Ƿ���ǰtime�����Ƶ����ǰһ���һ��time����Ƶ
                          time Ϊ0  ��������ʵʱ��Ƶ
				time �Ƿ�����Сֵ����
����ֵ: 
***************************************/
 int  Encode_Class::Send_Start(int time_len, int is_all, int channel) /*�������I ֡����ȡǰtime ���I ֡�� ��ǰһ��time���I ֡*/
{
	int alarm_iframe_index;
	struct tm curr_i_time;
	
	I_curr_tmp = frame_buff[channel].Ilist_curr;
	
	gmtime_r((time_t *)&frame_buff[channel].ikey_list[I_curr_tmp].time_stamp, &curr_i_time);
	printf("hour = %d minute = %d second = %d  \n", curr_i_time.tm_hour + 8, curr_i_time.tm_min, curr_i_time.tm_sec);
	log_class.Write_Debug_Log(0, "%02dh/%02dm/%02ds  ", curr_i_time.tm_hour + 8, curr_i_time.tm_min, curr_i_time.tm_sec);

	if(time_len == 0) /*�������ʵʱ��Ƶ ???*/
	{
		frame_buff[channel].alarm_time_len = 0;
		frame_buff[channel].alarm_time_end = 0;
		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[I_curr_tmp].iframe_offset;
		printf("read offset = %ld \n", frame_buff[channel].read_offset_before);
		return 0;
	}

	//printf("curr i time = %lld   ", frame_buff[channel].ikey_list[I_curr_tmp].time_stamp);
	if(is_all)     /*��ʾ��Ҫ��ȡǰtime �����Ƶ�������Ե�ǰI֡ƫ����Ϊtime �����λ��*/
	{
		
		frame_buff[channel].alarm_time_len = time_len;   
		frame_buff[channel].alarm_time_end = 1;  	
		frame_buff[channel].alarm_end_offset = frame_buff[channel].ikey_list[I_curr_tmp].iframe_offset; /*����ǰtime �����Ƶ����λ�ã���ǰI ֡*/

		alarm_iframe_index = Find_Alarm_Iframe(time_len, channel);
		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[alarm_iframe_index].iframe_offset;
	}
	else            /*��ʾ��Ҫ��ȡǰһ��time�����Ƶ����һ��time �����Ƶ��
				��Ҫ�ڱ���¼���ͬʱȷ��time ����Ƶ�Ľ���λ��				*/
	{
		frame_buff[channel].alarm_time_len = time_len;                    /*��¼��Ƶʱ�� time��*/
		frame_buff[channel].alarm_time_end = 0;  	                             /*��ʾtime �����Ƶ����λ��ûȷ��*/
		
		/****************************
		ͨ��I ֡���������ҵ�������Ƶ����ʼI ֡λ��
		���û���ҵ��� ?˵��I ֡�б��л�����û��ǰtime �����Ƶ
		*****************************/
		alarm_iframe_index = Find_Alarm_Iframe(time_len/2, channel);
		
		frame_buff[channel].alarm_start_time =  frame_buff[channel].ikey_list[alarm_iframe_index].time_stamp;     /*������Ƶ����ʼI ֡ʱ���*/

		frame_buff[channel].read_offset_before = frame_buff[channel].ikey_list[alarm_iframe_index].iframe_offset;  /*��������Ƶtime ����ʼλ��*/
		frame_buff[channel].alarm_end_offset = 0;
	}

	//printf(" alarm time = %lld \n", frame_buff[channel].ikey_list[alarm_iframe_index].time_stamp);
	printf("I_ccur = %d, alarm_iframe_index = %d ,read_offset = %ld   end_offset = %ld\n", frame_buff[channel].Ilist_curr, alarm_iframe_index, frame_buff[channel].read_offset_before, frame_buff[channel].alarm_end_offset);

	return 0;
}


/*******************************************************
�������:count  ����ȡ�������ֽ���

�����read_offset_before ��ʼ��ȡ������Ƶtime �� ����Ƶ

���ز���:data  
				end_flag : trueΪtime����Ƶ��ȡ��ϣ�false Ϊtime����Ƶδ��ȡ���


��Ҫ���ƶ���������  ��  д �������ݵ�ͬ��    ��ô����?????

ʵʱ�Ƿ���Ҫ�ж϶���д��ƫ���Ƿ���ȣ�������һ������д���ٶȲ�һ��

����ֵ: 
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
	
	*data = frame_buff[channel].buffer_start + frame_buff[channel].read_offset_before;   /*��ȡ����ָ��ĵ�ַ �����Բ���Ҫcopy ��ȥ*/
	
	
	if(frame_buff[channel].alarm_time_end == 0)/*---δ�ҵ���Ƶ�Ľ���λ��---*/
	 {						  /*��ȡcount ���ֽ�*/
		*end_flag = 0; 
		if (write_offset_tmp == frame_buff[channel].read_offset_before)//������д���ȴ�д���� ����0 ���ֽ�; 
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
				frame_buff[channel].read_offset_before += len;		/* ��ʱ��ƫ��׷��дƫ�� */
	
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


	/*	-------�ҵ�time ����Ƶ�Ľ���λ��------
	�Ƚϱ�����Ƶ��ʱ������Ƶ����λ��  �� ������Ƶ���ݵĽ���λ��
	û�е��ڵ�����ɣ� ���ھ��ǻ�ȡtime ���ڱ�����Ƶ����
	д���ݻ�׷�϶������� ??
	*/
	//usleep(1000*2);
	if(frame_buff[channel].read_offset_before > frame_buff[channel].alarm_end_offset)  /*������Ƶ��ƫ�� ���� ������Ƶ����ƫ��*/
	{
#if  0
		if(write_offset_tmp >= frame_buff.read_offset_before   || (write_offset_tmp <= frame_buff.alarm_end_offset))
		{ 
			?;//д���ݵ�׷���˶�����   ��δ����� 
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
			frame_buff[channel].read_offset_before = 0; 	/*����Ƶ������Ķ��ˣ��ٴ�0 ��ʼ��*/
			return len;
		}
		
	}
	else if(frame_buff[channel].read_offset_before < frame_buff[channel].alarm_end_offset)/*������Ƶ��ƫ�� С�� ������Ƶ����ƫ��*/
	{
		#if 0
		/**/
		if(write_offset_tmp >= frame_buff.read_offset_before && write_offset_tmp <= frame_buff.alarm_end_offset)
		{
			;//д���ݵ�׷���˶�����  ��δ�����
		}
		#endif
		len = frame_buff[channel].alarm_end_offset - frame_buff[channel].read_offset_before;
		if(len > count)
		{
			*end_flag = 0;  /*����time ����Ƶ��δ��*/
	
			frame_buff[channel].read_offset_before += count;
			return count;
		}
		else
		{
			*end_flag = 1;  /*����time ����Ƶ�����*/

			len = frame_buff[channel].alarm_end_offset - frame_buff[channel].read_offset_before;

			/*�ѻ�ȡ�걨��ʱ������Ƶ����*/
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
��дI ֡��ʱ�򣬼���һ���Ƿ�Ḳ��������I ֡ƫ��
flag = 0 Ϊ ��Ч I ֡ƫ��
flag = 1 Ϊ��ЧI ֡ƫ��
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
�������:  frame_data  ֡��Ƶ������ʼ��ַ
                            frame_len    ֡��Ƶ���ݳ���
                            isi_key        �Ƿ���I ֡
**************************************************/
void  Encode_Class::Save_Frame_Data(unsigned char *frame_data, int frame_len, bool is_ikey, int channel)
{
	int i_num;

	if(frame_buff[channel].write_offset >= frame_buff[channel].buffer_size)
	{	
		frame_buff[channel].write_offset = 0;
	}
	//printf("frame len = %d \n", frame_len);
	/*�Ƿ� I ֡��Ƶ���ݣ� ��¼I֡ʱ�估������ƫ�Ƶ�ַ*/
	if(is_ikey)                  
	{
		if(frame_buff[channel].Ilist_curr < (IFRAME_MAX_NUM - 1))   /*Ĭ��I ֡���������б���32��I ֡��ѭ��*/
		{
			frame_buff[channel].Ilist_curr++;
		}
		else
		{
			frame_buff[channel].Ilist_curr = 0;
		}
		
		i_num = frame_buff[channel].Ilist_curr;

		frame_buff[channel].ikey_list[i_num].iframe_offset = frame_buff[channel].write_offset;   /*��¼��I ֡����Ƶ����ƫ����*/
		frame_buff[channel].ikey_list[i_num].time_stamp = time(NULL);   			 /*��¼��I ֡��ʱ�仹�Ǽ�¼??*/
		//if(channel == 1)
			//printf(" i_curr = %d time = %lld  write offset = %ld \n", i_num,frame_buff[channel].ikey_list[i_num].time_stamp, frame_buff[channel].ikey_list[i_num].iframe_offset);
		
		//frame_buff.ikey_list[i_num].flag = 1;
		
		//Check_Iframe_valid(); //�����һ��I ֡�� ���I ֮֡���Ƿ�Ḳ�����е�I ֡
		#if 1
		if((frame_buff[channel].alarm_time_len != 0) &&               /*�Ƿ��б���time ��Ƶʱ��*/
			(frame_buff[channel].alarm_time_end == 0) &&		/*�Ƿ����ҵ�time ��Ƶ����λ��*/
			((frame_buff[channel].ikey_list[i_num].time_stamp - frame_buff[channel].alarm_start_time)  >= frame_buff[channel].alarm_time_len))  /*�Ƿ��б�����Ƶ�ϴ� time ����,�жϱ�����Ƶ���ݵĽ���λ��*/
		{	/*���ڵ��ڱ�����Ƶ�����ʱ����*/
			/*��¼������Ƶtime ���ڽ���λ��*/
			frame_buff[channel].alarm_end_offset = frame_buff[channel].write_offset;

			frame_buff[channel].alarm_time_end = 1;		/*���ҵ�����time ����Ƶ����λ��*/

			//printf("find over end offset ! read offset = %ld ,  end offset = %ld\n", frame_buff.alarm_end_offset,frame_buff.read_offset_before);
		}
		#endif

  
		//printf(" %ld = %d %d %d %d  \n", frame_buff.write_offset, frame_data[0],frame_data[1],frame_data[2],frame_data[3]);
	}

#if 0
	//��Ԥ¼������Ƶ�ϴ� 
 	//�������С             ���д�Ƿ���׷�϶� 
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
		else  if(rw_eq_flag != 1)/*��  д ƫ�����,    */ 
		{  
			w_r_len = frame_buff.buffer_size;
			;//r_w_len;					
		}   
		 
		if(frame_len >=  w_r_len)    /*˵���˿�д���ݺ�Ҫ��������ڶ����ݵ�ƫ�ƣ�   */   
		{    
	 		rw_eq_flag = 1;  // д׷�϶�ƫ�ƣ� ��������     ???    �� �ȿ���һ������Ҫ������Ƶ����  ??     
		} 
		else 
		{
			rw_eq_flag = 0;  
		}
		 
		 
	}

#endif

	//unsigned int write_offset_temp = frame_buff.write_offset;

	/*����֡��Ƶ���ݱ��浽������*/


	/*�Ƚϻ�����ʣ��Ĵ�С�ʹ���Ƶ���ݵĴ�С*/
	
	int leave_len = frame_buff[channel].buffer_size - frame_buff[channel].write_offset;
	
	if(leave_len > frame_len)
	{
		memcpy(frame_buff[channel].buffer_start + frame_buff[channel].write_offset, frame_data, frame_len);
		
		frame_buff[channel].write_offset += frame_len;			/*��һ��д�����ƫ��*/
		
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

		frame_buff[channel].write_offset = frame_len - leave_len;/*��һ��д�����ƫ��*/
	}
	
}



