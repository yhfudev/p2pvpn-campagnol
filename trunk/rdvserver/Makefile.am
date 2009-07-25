bin_PROGRAMS = campagnol_rdv
campagnol_rdv_SOURCES = campagnol_rdv.c rdv.h \
	net_socket.c net_socket.h \
	peer.c peer.h \
	session.c session.h \
	server.c server.h

campagnol_rdv_CPPFLAGS = -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\""
campagnol_rdv_CFLAGS = @COMMON_CFLAGS@
campagnol_rdv_LDADD = ../common/libcommon.a @RDV_LIBS@
