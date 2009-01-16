EXTRA_DIST = campagnol.8.in campagnol.conf.5.in campagnol_rdv.8.in

dist_man_MANS =
if BUILD_CLIENT
dist_man_MANS += campagnol.8 campagnol.conf.5
endif
if BUILD_SERVER
dist_man_MANS += campagnol_rdv.8
endif

do_subst = sed -e 's:@sysconfdir[@]:$(sysconfdir):g'\
	-e 's:@localstatedir[@]:$(localstatedir):g' \
	-e 's:@version[@]:$(PACKAGE_VERSION):g'

campagnol.8: campagnol.8.in
	$(do_subst) < campagnol.8.in > campagnol.8
campagnol.conf.5: campagnol.conf.5.in
	$(do_subst) < campagnol.conf.5.in > campagnol.conf.5
campagnol_rdv.8: campagnol_rdv.8.in
	$(do_subst) < campagnol_rdv.8.in > campagnol_rdv.8

CLEANFILES = campagnol.8 campagnol.conf.5 campagnol_rdv.8
