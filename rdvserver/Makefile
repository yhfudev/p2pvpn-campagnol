# The directory for installing executable
bindir=/usr/local/bin/
# The directory for installing the jar archive
pkgdatadir=/usr/local/share/campagnol/
# Root directory for the man page
mandir=/usr/local/share/man/

JAVAC=javac
JAR=$$(which jar 2>/dev/null)
ANT=$$(which ant 2>/dev/null)
JAVACFLAGS=-source 1.4 -target 1.4

PACKAGE_VERSION=0.3.1

do_subst = sed -e 's:@version[@]:$(PACKAGE_VERSION):g'

campagnol_rdv_sources = \
	net/sourceforge/campagnol/CampagnolGUI.java \
	net/sourceforge/campagnol/CampagnolServer.java \
	net/sourceforge/campagnol/ClientStruct.java \
	net/sourceforge/campagnol/Connection.java \
	net/sourceforge/campagnol/MsgServStruct.java \
	net/sourceforge/campagnol/OptionParser.java \
	net/sourceforge/campagnol/CampagnolRDV.java

campagnol_rdv_classes = \
	classes/net/sourceforge/campagnol/CampagnolGUI.class \
	classes/net/sourceforge/campagnol/CampagnolServer.class \
	classes/net/sourceforge/campagnol/ClientStruct.class \
	classes/net/sourceforge/campagnol/Connection.class \
	classes/net/sourceforge/campagnol/MsgServStruct.class \
	classes/net/sourceforge/campagnol/OptionParser.class \
	classes/net/sourceforge/campagnol/CampagnolRDV.class

CLEANFILES = campagnol_rdv.8 campagnol_rdv campagnol_rdv.jar classes/net/sourceforge/campagnol/*.class

all: campagnol_rdv campagnol_rdv.jar campagnol_rdv.8

campagnol_rdv.8: campagnol_rdv.8.in
	$(do_subst) < campagnol_rdv.8.in > campagnol_rdv.8

campagnol_rdv.jar: $(campagnol_rdv_sources)
	@if [ -x "$(ANT)" ]; then \
	  echo "$(ANT) jar" ; \
	  $(ANT) jar ; \
	else \
	  echo "$(JAVAC) -d classes $(JAVACFLAGS) $(campagnol_rdv_sources)" ; \
	  $(JAVAC) -d classes $(JAVACFLAGS) $(campagnol_rdv_sources) ; \
	  if [ -x "$(JAR)" ]; then \
	    echo "$(JAR) -cfm campagnol_rdv.jar MANIFEST.MF -C classes net" ; \
	    $(JAR) -cfm campagnol_rdv.jar MANIFEST.MF -C classes net ; \
	  else \
	    echo "mkdir -p classes/META-INF" ; \
	    mkdir -p classes/META-INF ; \
	    echo "cp MANIFEST.MF classes/META-INF" ; \
	    cp MANIFEST.MF classes/META-INF ; \
	    echo "cd classes && zip -r campagnol_rdv.jar META-INF net && mv campagnol_rdv.jar ../ && cd ../" ; \
	    cd classes && zip -r campagnol_rdv.jar META-INF net && mv campagnol_rdv.jar ../ && cd ../ ; \
	  fi \
	fi

campagnol_rdv:
	echo "#!/bin/sh" > campagnol_rdv
	echo "exec java -jar \"${pkgdatadir}/campagnol_rdv.jar\" \$${@}" >> campagnol_rdv
	chmod +x campagnol_rdv

clean:
	rm -f $(CLEANFILES)

install-bin: campagnol_rdv
	mkdir -p "$(bindir)"
	install campagnol_rdv "$(bindir)"
uninstall-bin:
	rm -f "$(bindir)/campagnol_rdv"

install-pkgdata: campagnol_rdv.jar
	mkdir -p "$(pkgdatadir)"
	install -m 0644 campagnol_rdv.jar "$(pkgdatadir)"
uninstall-pkgdata:
	rm -f "$(pkgdatadir)/campagnol_rdv.jar"

install-man: campagnol_rdv.8
	mkdir -p "$(mandir)/man8/"
	install -m 0644 campagnol_rdv.8 "$(mandir)/man8/"
uninstall-man:
	rm -f "$(mandir)/man8/campagnol_rdv.8"

install: install-bin install-pkgdata install-man
uninstall: uninstall-bin uninstall-pkgdata uninstall-man


.PHONY: all clean install uninstall \
	install-bin install-pkgdata install-man \
	uninstall-bin uninstall-pkgdata uninstall-man
