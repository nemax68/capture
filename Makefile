CC=arm-linux-gnueabihf-gcc

CFLAGS =-Iinc
CFLAGS +=-g
CFLAGS += -Wall
#CFLAGS += -lrt
#CFLAGS += -pthread


SRCS = capture.c
SRCS += jpeg.c
SRCS += cam_ctrl.c
SRCS += term.c

capture: $(SRCS)
	$(CC) $(SRCS) -o cap $(CFLAGS)

clean:
	rm *.o
	rm cap
	rm *.*~


