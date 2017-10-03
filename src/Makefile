NVCC        = nvcc
NVCCFLAGS   = -O3 --compiler-options '-fPIC' --shared -Xcompiler -Wall -arch=sm_35 -lrt -lcudart -lcufft -lineinfo
CC          = gcc
# The user should specify, on the 'make' command line :
# S6_LOCATION="-D SOURCE_DIBAS"
# or
# S6_LOCATION="-D SOURCE_S6"
S6_LOCATION = 
#S6_LIB_CCFLAGS     = -g -O3 -fPIC -shared -lstdc++ -mavx -msse4 
S6_LIB_CCFLAGS     = -g -O3 -fPIC -shared -lstdc++ \
		     -I. -I$(CUDA_DIR)/include -I/usr/local/include -I/usr/include/cfitsio -I. \
		     -L. -L/usr/local/lib -L/usr/lib64 -L/usr/local/lib/hiredis \
		     -lhashpipe -lS6GPU -lrt -lhiredis -lcfitsio -lazzatoradec -laoscram -lm
S6OBS_AO_PROG_CCFLAGS = -g -lstdc++ \
		     -I/usr/include/tcl \
		     -I/usr/local/include \
		     -I/usr/local/include/ao \
		     -L/usr/local/lib \
		     -lhiredis -lazzatoradec -laoscram -lm
S6OBS_GBT_PROG_CCFLAGS = -g -lstdc++ \
             -L/usr/lib64/mysql \
             -I/usr/include/mysql \
		     -I/usr/local/include \
		     -I/usr/local/cuda/include \
		     -L/usr/local/lib \
		     -lhiredis -lmysqlclient -lm serendip6.so
S6OBS_TEST_OBS_GBT_PROG_CCFLAGS = -g -O3 -fPIC -lstdc++ -mavx -msse4 \
             -L/usr/lib64/mysql \
		     -I. -I$(CUDA_DIR)/include -I/usr/local/include \
		     -L. -L/usr/local/lib \
             -I/usr/include/mysql \
		     -lmysqlclient serendip6.so
S6GPUTEST_CCFLAGS  = -g \
		     -I. -I/usr/local/cuda/include \
		     -L. -L/usr/local/cuda/lib64 \
		     -lcudart -lrt -lcufft -lS6GPU  \
		     -lcfitsio serendip6.so
S6EXTRACT_PROG_CCFLAGS  = -g -lstdc++ \
		     -I. -I/usr/local/cuda/include
S6_CHAN2IF_CCFLAGS = -g -lstdc++

S6GPU_LIB_TARGET    = libS6GPU.so
S6GPU_LIB_SOURCES   = s6GPU.cu
S6GPU_LIB_INCLUDES  = s6GPU.h \
		      stopwatch.hpp

S6EXTRACT_PROG_TARGET   = s6_extract_coarse_chan
S6EXTRACT_PROG_SOURCES  = s6_extract_coarse_chan.cpp
S6EXTRACT_PROG_INCLUDES = s6_databuf.h

S6_LIB_TARGET   = serendip6.so
S6_LIB_SOURCES  = s6_databuf.cpp \
		  s6_gen_fake_data.cpp \
		  s6_fake_net_thread.cpp \
		  s6_gpu_thread.cpp \
		  s6_output_thread.cpp \
		  s6_obs_data.cpp \
		  s6_obs_data_gbt.cpp \
		  s6_etfits.cpp \
		  s6_time.c
S6_LIB_INCLUDES = s6_etfits.h \
		  s6_databuf.h

S6OBS_AO_PROG_TARGET   = s6_observatory_ao
S6OBS_AO_PROG_SOURCES  = s6_observatory_ao.c \
		      s6_obsaux_ao.c \
		      s6_time.c
S6OBS_AO_PROG_INCLUDES = s6_obsaux.h \
		      s6_time.h

S6OBS_GBT_PROG_TARGET   = s6_observatory_gbt
S6OBS_GBT_PROG_SOURCES  = s6_observatory_gbt.c \
		      s6_obsaux_gbt.c
S6OBS_GBT_PROG_INCLUDES = s6_obsaux_gbt.h

S6OBS_TEST_OBS_GBT_PROG_TARGET   = test_obs
S6OBS_TEST_OBS_GBT_PROG_SOURCES  = test_obs.cpp

S6_CHAN2IF_TARGET  = s6_chan2if
S6_CHAN2IF_SOURCES = s6_chan2if.cpp

S6_SCRIPT_TARGET = s6_init.sh

all_ao:  $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6GPUTEST_PROG_TARGET) $(S6EXTRACT_PROG_TARGET) $(S6OBS_AO_PROG_TARGET)  $(S6_CHAN2IF_TARGET)  
all_gbt: $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6GPUTEST_PROG_TARGET) $(S6EXTRACT_PROG_TARGET) $(S6OBS_GBT_PROG_TARGET) $(S6_CHAN2IF_TARGET)

cleo_hello: cleo_hello.cpp
	gcc -o cleo_hello cleo_hello.cpp

$(S6GPU_LIB_TARGET):      $(S6GPU_LIB_SOURCES) $(S6GPU_LIB_INCLUDES) ;           $(NVCC) -o $(S6GPU_LIB_TARGET) $(S6GPU_LIB_SOURCES) $(NVCCFLAGS) $(S6_LOCATION) 

$(S6GPUTEST_PROG_TARGET): $(S6GPUTEST_PROG_SOURCES) $(S6GPUTEST_PROG_INCLUDES) ; $(CC)   -o $(S6GPUTEST_PROG_TARGET) $(S6GPUTEST_PROG_SOURCES) $(S6GPUTEST_CCFLAGS) $(S6_LOCATION) 

$(S6_LIB_TARGET):         $(S6_LIB_SOURCES) $(S6_LIB_INCLUDES) ;                 $(CC)   -o $(S6_LIB_TARGET) $(S6_LIB_SOURCES) $(S6_LIB_CCFLAGS) $(S6_LOCATION)

$(S6OBS_PROG_TARGET):     $(S6OBS_PROG_SOURCES) $(S6OBS_PROG_INCLUDES) ;         $(CC)   -o $(S6OBS_PROG_TARGET) $(S6OBS_PROG_SOURCES) $(S6OBS_PROG_CCFLAGS) $(S6_LOCATION)

$(S6OBS_AO_PROG_TARGET): $(S6OBS_AO_PROG_SOURCES) $(S6OBS_AO_PROG_INCLUDES) ;    $(CC)   -o $(S6OBS_AO_PROG_TARGET) $(S6OBS_AO_PROG_SOURCES) $(S6OBS_AO_PROG_CCFLAGS)

$(S6OBS_GBT_PROG_TARGET): $(S6OBS_GBT_PROG_SOURCES) $(S6OBS_GBT_PROG_INCLUDES) ; $(CC)   -o $(S6OBS_GBT_PROG_TARGET) $(S6OBS_GBT_PROG_SOURCES) $(S6OBS_GBT_PROG_CCFLAGS)

$(S6OBS_TEST_OBS_GBT_PROG_TARGET): $(S6OBS_TEST_OBS_GBT_PROG_SOURCES) ;          $(CC)   -o $(S6OBS_TEST_OBS_GBT_PROG_TARGET) $(S6OBS_TEST_OBS_GBT_PROG_SOURCES) $(S6OBS_TEST_OBS_GBT_PROG_CCFLAGS)

$(S6EXTRACT_PROG_TARGET): $(S6EXTRACT_PROG_SOURCES) $(S6EXTRACT_PROG_INCLUDES) ; $(CC)   -o $(S6EXTRACT_PROG_TARGET) $(S6EXTRACT_PROG_SOURCES) $(S6EXTRACT_PROG_CCFLAGS) $(S6_LOCATION)

$(S6_CHAN2IF_TARGET): $(S6_CHAN2IF_SOURCES) $(S6_CHAN2IF_INCLUDES) ;             $(CC)   -o $(S6_CHAN2IF_TARGET) $(S6_CHAN2IF_SOURCES) $(S6_CHAN2IF_CCFLAGS)

tags:
	ctags -R .

clean: ; rm -f $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET) $(S6OBS_AO_PROG_TARGET) $(S6OBS_GBT_PROG_TARGET) $(S6GPUTEST_PROG_TARGET) $(S6EXTRACT_PROG_TARGET) tags

prefix=/usr/local
ETCDIR=$(prefix)/etc
LIBDIR=$(prefix)/lib
BINDIR=$(prefix)/bin

install-etc: obsPosition.dat utcToUt1.dat s6_ETFITS_template.txt
	mkdir -p "$(DESTDIR)$(ETCDIR)"
	install -p $^ "$(DESTDIR)$(ETCDIR)"

install-lib: $(S6GPU_LIB_TARGET) $(S6_LIB_TARGET)
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	install -p $^ "$(DESTDIR)$(LIBDIR)"

install-bin: $(S6OBS_PROG_TARGET) 
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -p $^ "$(DESTDIR)$(BINDIR)"

install-script: $(S6_SCRIPT_TARGET) 
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -p $^ "$(DESTDIR)$(BINDIR)"

install: install-etc install-lib install-bin install-script

.PHONY: all tags clean install install-lib install-etc install-bin
# vi: set ts=8 noet :
