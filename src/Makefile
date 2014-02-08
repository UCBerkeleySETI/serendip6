CC          = gcc
CCFLAGS     = -g -fPIC -shared -lstdc++ -fpermissive \
              -I../../s6GPU/src/ -I/usr/local/cuda/include -I../../hiredis/ -I/usr/include/tcl8.5 -I/home/jeffc/lib/include -I/home/jeffc/lib/include/aoscram \
              -L../../s6GPU/src/ -L/home/jeffc/lib/lib \
              -lhashpipe -lS6GPU -lrt -lhiredis -lcfitsio -lazzatoradec -laoscram -lm
CCFLAGS2     = -g -lstdc++ \
              -I../../hiredis/ -I/usr/include/tcl8.5 -I/home/jeffc/lib/include -I/home/jeffc/lib/include/aoscram \
              -L/home/jeffc/lib/lib \
              -lhiredis -lazzatoradec -laoscram -lm

LIBTARGET   =   serendip6.so
LIBSOURCES  =   s6_databuf.cpp \
				s6_gen_fake_data.cpp \
				s6_fake_net_thread.cpp \
				s6_gpu_thread.cpp \
				s6_output_thread.cpp \
                s6_etfits.cpp
LIB_INCLUDES  = s6_etfits.h \
                s6_databuf.h

PROGTARGET = s6_observatory
PROGSOURCES = s6_observatory.c \
              s6_obsaux.c
PROG_INCLUDES = s6obsaux.h

all: $(LIBTARGET) $(PROGTARGET) 

$(LIBTARGET): $(LIBSOURCES) $(LIB_INCLUDES) ; $(CC) -o $(LIBTARGET) $(LIBSOURCES) $(CCFLAGS)

$(PROGTARGET): $(PROCSOURCES) $(PROC_INCLUDES) ; $(CC) -o $(PROGTARGET) $(PROGSOURCES) $(CCFLAGS2)

clean: ; rm $(LIBTARGET) $(PROGTARGET)
