NVCC        = nvcc
NVCCFLAGS   = -O3 --compiler-options '-fPIC' --shared -Xcompiler -Wall -arch=sm_35 -lrt -lcudart -lcufft
CC          = gcc
S6_LIB_CCFLAGS     = -g -fPIC -shared -lstdc++ \
		     -I. -I$(CUDA_DIR)/include -I/usr/local/include \
		     -L. -L/use/local/lib \
		     -lhashpipe -lS6GPU -lrt -lhiredis -lcfitsio -lazzatoradec -laoscram -lm
S6OBS_PROG_CCFLAGS = -g -lstdc++ \
		     -I/usr/include/tcl \
		     -I/usr/local/include \
		     -I/usr/local/include/ao \
		     -L/usr/local/lib \
		     -lhiredis -lazzatoradec -laoscram -lm
S6GPUTEST_CCFLAGS  = -g \
		     -I. -I/usr/local/cuda/include \
		     -L. -L/usr/local/cuda/lib64 \
		     -lcudart -lrt -lcufft -lS6GPU  \
		     -lcfitsio serendip6.so

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

tags:
	ctags -R .

clean: ; rm -f $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6OBS_PROG_TARGET) $(S6GPUTEST_PROG_TARGET) tags

prefix=/usr/local
ETCDIR=$(prefix)/etc
LIBDIR=$(prefix)/lib
BINDIR=$(prefix)/bin

install-etc: obsPosition.dat utcToUt1.dat s6_ETFITS_template.txt
	mkdir -p "$(DESTDIR)$(ETCDIR)"
	install $^ "$(DESTDIR)$(ETCDIR)"

install-lib: $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET)
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	install $^ "$(DESTDIR)$(LIBDIR)"

install-bin: $(S6OBS_PROG_TARGET) 
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install $^ "$(DESTDIR)$(BINDIR)"

install: install-etc install-lib install-bin

.PHONY: all tags clean install install-lib install-etc install-bin
# vi: set ts=8 noet :
