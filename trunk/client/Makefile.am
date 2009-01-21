AUTOMAKE_OPTIONS = subdir-objects

dist_sysconf_DATA = campagnol.conf

bin_PROGRAMS = campagnol
campagnol_SOURCES = bf_rate_limiter.c bf_rate_limiter.h \
	bss_fifo.c bss_fifo.h \
	campagnol.c campagnol.h \
	communication.c communication.h \
	config_parser.c config_parser.h \
	configuration.c configuration.h \
	net_socket.c net_socket.h \
	peer.c peer.h \
	pthread_wrap.h \
	rate_limiter.c rate_limiter.h \
	sem_wrap.h \
	tun_device.h
if HAVE_LINUX
campagnol_SOURCES += tun_device_linux.c
endif
if HAVE_FREEBSD
campagnol_SOURCES += tun_device_freebsd.c
endif
if HAVE_OPENBSD
campagnol_SOURCES += tun_device_openbsd.c
endif
campagnol_CPPFLAGS = -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\"" @OPENSSL_CFLAGS@
campagnol_CFLAGS = -Wall -Wstrict-prototypes
campagnol_LDADD = $(LIBOBJS) ../common/libcommon.a @OPENSSL_LIBS@ -lm

