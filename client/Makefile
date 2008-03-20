CC=gcc
ifdef NODEBUG
CFLAGS=-O3 -D_GNU_SOURCE -Wall -Wstrict-prototypes
else
CFLAGS=-g3 -O0 -D_GNU_SOURCE -Wall -Wstrict-prototypes
endif

LDFLAGS=-L/usr/local/lib -lpthread -lrt -lssl -lcrypto

SRCS=$(wildcard *.c)
OBJETS=$(SRCS:%.c=%.o)

all: campagnol

campagnol: $(OBJETS)
	$(CC) $(LDFLAGS) $(OBJETS) -o $@

clean:
	rm -f campagnol *~ *.o

depend:
	makedepend -m -Y. $(SRCS)

# DO NOT DELETE

bss_fifo.o: campagnol.h configuration.h bss_fifo.h pthread_wrap.h
campagnol.o: campagnol.h configuration.h tun_device.h net_socket.h
campagnol.o: communication.h
communication.o: campagnol.h configuration.h communication.h pthread_wrap.h
communication.o: net_socket.h peer.h
configuration.o: campagnol.h configuration.h
net_socket.o: campagnol.h configuration.h communication.h net_socket.h
peer.o: campagnol.h configuration.h peer.h pthread_wrap.h bss_fifo.h
pthread_wrap.o: campagnol.h configuration.h pthread_wrap.h
tun_device.o: campagnol.h configuration.h tun_device.h
