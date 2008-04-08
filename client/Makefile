CC=gcc

SRC=src/
OBJ=obj/
INCLUDE=-Iinclude/
DEP=.dep/
BIN=bin/

FLAGS=-D_GNU_SOURCE -Wall -Wstrict-prototypes $(INCLUDE)
ifdef NODEBUG
CFLAGS=-O3 $(FLAGS)
else
CFLAGS=-g3 -O0 $(FLAGS)
endif
LDFLAGS=-L/usr/local/lib -lpthread -lrt -lssl -lcrypto

CSOURCES=bss_fifo.c campagnol.c communication.c configuration.c net_socket.c peer.c pthread_wrap.c tun_device.c
OBJFILES=$(CSOURCES:%.c=$(OBJ)%.o)
DEPFILES=$(OBJFILES:$(OBJ)%.o=$(DEP)%.d)
BINFILE=$(BIN)campagnol

.PHONY: all clean


$(DEP)%.d: $(SRC)%.c
	@echo "Calculating dependencies for $<"
	@$(CC) -MM -MP -MF"$@" -MT"$(@:$(DEP)%.d=$(OBJ)%.o)" $(FLAGS) $< 

$(OBJ)%.o: $(SRC)%.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BINFILE): $(DEPFILES) $(OBJFILES)
	@echo "[LNK] $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJFILES)

all: $(BINFILE)

clean:
	rm -f $(OBJFILES) $(DEPFILES) $(BINFILE)

ifneq ($(firstword $(MAKECMDGOALS)),clean)
-include $(DEPFILES)
endif
