TARGET  := libtinywf.a

#compile and lib parameter

CC      := gcc
AR      = ar
RANLIB  = ranlib
LIBS    :=
LDFLAGS :=
DEFINES :=
INCLUDE := -I.
CFLAGS  := -g -Wall -O3 $(DEFINES) $(INCLUDE)
CXXFLAGS:= $(CFLAGS) -DHAVE_CONFIG_H

#i think you should do anything here


#source file
SOURCE  := $(wildcard *.c) $(wildcard *.cc)
OBJS    := $(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(SOURCE)))

.PHONY : everything objs clean veryclean rebuild

everything : $(TARGET)

all : $(TARGET)

objs : $(OBJS)

rebuild: veryclean everything

clean :
		rm -fr *.o
		rm -fr $(TARGET)

$(TARGET) : $(OBJS)
		$(AR) cru $(TARGET) $(OBJS)
		$(RANLIB) $(TARGET)
