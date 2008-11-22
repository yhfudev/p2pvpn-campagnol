bin_PROGRAMS = campagnol
campagnol_SOURCES = bss_fifo.c campagnol.c communication.c configuration.c log.c net_socket.c peer.c config_parser.c rate_limiter.c bf_rate_limiter.c
if HAVE_LINUX
campagnol_SOURCES += tun_device_linux.c
endif
if HAVE_FREEBSD
campagnol_SOURCES += tun_device_freebsd.c
endif
if HAVE_OPENBSD
campagnol_SOURCES += tun_device_openbsd.c
endif
campagnol_CPPFLAGS = -I"$(srcdir)/../include" -DSYSCONFDIR="\"$(abspath $(sysconfdir))\"" -DLOCALSTATEDIR="\"$(abspath $(localstatedir))\"" @OPENSSL_CFLAGS@
campagnol_CFLAGS = -Wall -Wstrict-prototypes
campagnol_LDADD = @OPENSSL_LIBS@ -lm

