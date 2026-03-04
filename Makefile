CC=arm-himix410-linux-gcc
CPP=arm-himix410-linux-g++

ROOTDIR = $(PWD)
SRCDIR = $(ROOTDIR)/src
SRCPLATDIR = $(ROOTDIR)/src/platform
SDK_INCLUDE = $(ROOTDIR)/include/hisi
OSA_INCLUDE = $(ROOTDIR)/include/osa
PLAT_INCLUDE = $(ROOTDIR)/include/plat
ALGO_INCLUDE = $(ROOTDIR)/include/algo
LIB_DIR = $(ROOTDIR)/lib

TARGET := demon
SRC := $(shell find $(SRCDIR)/ -maxdepth 2 -iname "*.c")
OBJ_C = $(SRC:%.c=%.o)
SRC_CPP := $(shell find $(SRCDIR)/ -maxdepth 2 -iname "*.cpp")
OBJ_CPP = $(SRC_CPP:%.cpp=%.o)

CPPFLAGS := -std=c++11
CFLAGS += -I$(SDK_INCLUDE) -I$(OSA_INCLUDE) -I$(PLAT_INCLUDE) -I$(ALGO_INCLUDE)
CFLAGS += -mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4 -fno-aggressive-loop-optimizations
LDFLAGS += -lpthread -lm -ldl
LDFLAGS += -L$(LIB_DIR) -lsvpruntime -l_hiae -l_hiawb -l_hiawb_natura -lisp -l_hiacs -l_hiir_auto -l_hicalcflicker -l_hidrc -l_hildci -l_hidehaze -lmpi -live -lmd -lnnie -lsns_imx307 -ltde -lVoiceEngine -lupvqe -ldnvqe -lsecurec 
LDFLAGS += -lOmniStream

# Opencv
LDFLAGS += -L$(ROOTDIR)/lib/opencv
LDFLAGS += -lopencv_core -lopencv_imgproc -lopencv_objdetect -lopencv_calib3d -lopencv_flann -lopencv_imgcodecs
LDFLAGS += -lstdc++ -lsupc++

%.o: %.c
	@$(CC) -g -O0 -o $@ -c $< $(CFLAGS)
%.o: %.cpp
	@$(CPP) -g -O0 -c $(CFLAGS) $(CPPFLAGS)  $< -o $@

all:$(OBJ_C) $(OBJ_CPP)
	@$(CPP) -g -O0 -o $(TARGET) $^ $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) 

clean:
	@rm -f $(TARGET) $(OBJ_C) $(OBJ_CPP)
