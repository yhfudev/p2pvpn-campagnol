EXTRA_DIST = campagnol.8.in campagnol.conf.5.in campagnol_rdv.8.in

man8_MANS =
if BUILD_CLIENT
man8_MANS += campagnol.8
man5_MANS = campagnol.conf.5
endif
if BUILD_SERVER
man8_MANS += campagnol_rdv.8
endif

do_subst = sed -e 's:@sysconfdir[@]:$(sysconfdir):g'\
	-e 's:@localstatedir[@]:$(localstatedir):g' \
	-e 's:@version[@]:$(PACKAGE_VERSION):g'

campagnol.8: campagnol.8.in
	$(do_subst) < $< > $@
campagnol.conf.5: campagnol.conf.5.in
	$(do_subst) < $< > $@
campagnol_rdv.8: campagnol_rdv.8.in
	$(do_subst) < $< > $@

CLEANFILES = campagnol.8 campagnol.conf.5 campagnol_rdv.8
