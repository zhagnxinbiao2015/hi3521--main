# Hisilicon Hi3516 sample Makefile

include ../Makefile.param
#ifeq ($(SAMPLE_PARAM_FILE), )
#     SAMPLE_PARAM_FILE:=../Makefile.param
#     include $(SAMPLE_PARAM_FILE)
#endif

# target source

#modify by zhangxinbiao 

SRC  := $(wildcard *.c) 
OBJ  := $(SRC:%.c=%.o)

TARGET := $(OBJ:%.o=%)
.PHONY : clean all


MPI_LIBS := $(REL_LIB)/libmpi.a
MPI_LIBS += $(REL_LIB)/libhdmi.a


RTP_RECV_OBJ := rtp_recv_dec.o

RTP_SEND_OBJ := rtp_main.o

#RTP_SEND_OBJ := rtpcliend.o

EXE := rtp_recv_dec rtp_send_cod

all: $(EXE)

rtp_recv_dec: $(RTP_RECV_OBJ) $(COMM_OBJ)
	$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ $(MPI_LIBS) $(AUDIO_LIBA) $(JPEGD_LIBA)


rtp_send_cod: $(RTP_SEND_OBJ) $(COMM_OBJ)
	$(CC) $(CFLAGS) -lpthread -lm -o $@ $^ $(MPI_LIBS) $(AUDIO_LIBA) $(JPEGD_LIBA)
	
clean:
	@rm -f $(TARGET)
	@rm -f $(OBJ)
	@rm -f $(COMM_OBJ)
	@rm -f $(EXE)

cleanstream:
#	@rm -f *.h264
#	@rm -f *.jpg
#	@rm -f *.mjp
#	@rm -f *.mp4
