dist_sysconf_DATA = campagnol.conf

bin_PROGRAMS = campagnol
campagnol_SOURCES = bf_rate_limiter.c bf_rate_limiter.h \
	campagnol.c campagnol.h \
	communication.c communication.h \
	configuration.c configuration.h \
	dtls_utils.c dtls_utils.h \
	net_socket.c net_socket.h \
	peer.c peer.h \
	rate_limiter.c rate_limiter.h \
	tun_device_common.c tun_device.h
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
campagnol_CFLAGS = @COMMON_CFLAGS@
campagnol_LDADD = ../common/libcommon.a -lm @OPENSSL_LIBS@ @CLIENT_LIBS@

