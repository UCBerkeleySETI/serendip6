NVCC        = nvcc
NVCCFLAGS   = -O3 --compiler-options '-fPIC' --shared -Xcompiler -Wall -arch=sm_35 -lrt -lcufft
CC          = gcc
S6_LIB_CCFLAGS     = -g -fPIC -shared -lstdc++ \
                     -I. -I/usr/local/cuda/include -I../../hiredis/ -I/usr/include/tcl8.5 \
                     -I/home/jeffc/local/include -I/home/jeffc/local/include/aoscram \
                     -L. -L/home/jeffc/local/lib \
                     -lhashpipe -lS6GPU -lrt -lhiredis -lcfitsio -lazzatoradec -laoscram -lm
S6OBS_PROG_CCFLAGS = -g -lstdc++ \
                     -I../../hiredis/ -I/usr/include/tcl8.5 -I/home/jeffc/local/include \
                     -I/home/jeffc/local/include/aoscram \
                     -L/home/jeffc/local/lib \
                     -lhiredis -lazzatoradec -laoscram -lm
S6GPUTEST_CCFLAGS  = -g \
                     -I. -I/home/jeffc/build/serendip6/src -I/usr/local/cuda/include \
                     -L. -L/home/jeffc/build/serendip6/src -L/usr/local/cuda/bin/../lib64 \
                     -lcudart -lrt -lcufft -lS6GPU  \
                     -lcfitsio -lserendip6

S6GPU_LIB_TARGET    = libS6GPU.so
S6GPU_LIB_SOURCES   = s6GPU.cu
S6GPU_LIB_INCLUDES  = s6GPU.h \
                      stopwatch.hpp

S6GPUTEST_PROG_TARGET   = s6GPU_test
S6GPUTEST_PROG_SOURCES  = s6GPU_test.cpp \
                          s6_fake_data.cpp
S6GPUTEST_PROG_INCLUDES = s6GPU.h \
                          s6GPU_test.h

S6_LIB_TARGET   = serendip6.so
S6_LIB_SOURCES  = s6_databuf.cpp \
				  s6_gen_fake_data.cpp \
				  s6_fake_net_thread.cpp \
                  s6_fake_data.cpp \
				  s6_gpu_thread.cpp \
				  s6_output_thread.cpp \
                  s6_obs_data.cpp \
                  s6_etfits.cpp \
                  s6_time.c 
S6_LIB_INCLUDES = s6_etfits.h \
                  s6_databuf.h

S6OBS_PROG_TARGET   = s6_observatory
S6OBS_PROG_SOURCES  = s6_observatory.c \
                      s6_obsaux.c \
                      s6_time.c
S6OBS_PROG_INCLUDES = s6_obsaux.h \
                      s6_time.h

all: $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6GPUTEST_PROG_TARGET) $(S6OBS_PROG_TARGET) 

$(S6GPU_LIB_TARGET): $(S6GPU_LIB_SOURCES) $(S6GPU_LIB_INCLUDES)   ; $(NVCC) -o $(S6GPU_LIB_TARGET) $(S6GPU_LIB_SOURCES) $(NVCCFLAGS)

$(S6GPUTEST_PROG_TARGET): $(S6GPUTEST_PROG_SOURCES) $(S6GPUTEST_PROG_INCLUDES) ; $(CC) -o $(S6GPUTEST_PROG_TARGET) $(S6GPUTEST_PROG_SOURCES) $(S6GPUTEST_CCFLAGS)

$(S6_LIB_TARGET): $(S6_LIB_SOURCES) $(S6_LIB_INCLUDES) ; $(CC) -o $(S6_LIB_TARGET) $(S6_LIB_SOURCES) $(S6_LIB_CCFLAGS)

$(S6OBS_PROG_TARGET): $(S6OBS_PROG_SOURCES) $(S6OBS_PROG_INCLUDES) ; $(CC) -o $(S6OBS_PROG_TARGET) $(S6OBS_PROG_SOURCES) $(S6OBS_PROG_CCFLAGS)

clean: ; rm $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6OBS_PROG_TARGET) $(S6GPUTEST_PROG_TARGET)
