bin_PROGRAMS = campagnol
campagnol_SOURCES = bss_fifo.c  campagnol.c  communication.c  configuration.c  log.c  net_socket.c  peer.c  pthread_wrap.c  tun_device.c
campagnol_CPPFLAGS = -I"$(srcdir)/../include" @OPENSSL_CFLAGS@
campagnol_CFLAGS = -Wall -Wstrict-prototypes
campagnol_LDFLAGS = @OPENSSL_LIBS@


EXTRA_DIST = tun_device_linux.c tun_device_freebsd.c
