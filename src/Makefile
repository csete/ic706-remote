#
# 'make'        build executables
# 'make clean'  removes all .o and executable files
#

CC = gcc
CFLAGS = -Wall -Wextra -O3 `pkg-config --cflags --libs portaudio-2.0 opus`
LIBS = -lm `pkg-config --cflags --libs portaudio-2.0 opus`

#INCLUDES = -I./src/
#LFLAGS = 

# IC-706 control server
IS_SRCS = ic706_server.c common.c common.h
IS_OBJS = $(IS_SRCS:.c=.o)
IS_MAIN = ic706_server

# IC-706 control client
IC_SRCS = ic706_client.c common.c common.h
IC_OBJS = $(IC_SRCS:.c=.o)
IC_MAIN = ic706_client

# Audio server
AS_SRCS = audio_server.c audio_util.c audio_util.h common.c common.h
AS_OBJS = $(AS_SRCS:.c=.o)
AS_MAIN = audio_server

# Audio client
AC_SRCS = audio_client.c audio_util.c audio_util.h common.c common.h
AC_OBJS = $(AC_SRCS:.c=.o)
AC_MAIN = audio_client

# serial gateway (not built by default)
SG_SRCS = serial_gateway.c common.c common.h
SG_OBJS = $(SG_SRCS:.c=.o)
SG_MAIN = serial_gateway

all:    $(IS_MAIN) $(IC_MAIN) $(AS_MAIN) $(AC_MAIN)


$(IS_MAIN): $(IS_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(IS_MAIN) $(IS_OBJS) $(LFLAGS) $(LIBS)

$(IC_MAIN): $(IC_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(IC_MAIN) $(IC_OBJS) $(LFLAGS) $(LIBS)

$(AS_MAIN): $(AS_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(AS_MAIN) $(AS_OBJS) $(LFLAGS) $(LIBS)

$(AC_MAIN): $(AC_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(AC_MAIN) $(AC_OBJS) $(LFLAGS) $(LIBS)

$(SG_MAIN): $(SG_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(SG_MAIN) $(SG_OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(AS_MAIN) $(AC_MAIN) $(IS_MAIN) $(IC_MAIN) $(SG_MAIN)

.PHONY: depend clean
