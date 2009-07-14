#define git_repodir /home/panosl/panos/build
%define git_repo asterisk
%define git_head 16xrg

%define version %git_get_ver
%define distsuffix xrg
%define release %git_get_rel

# %git_get_version TODO

%define _requires_exceptions perl(Carp::Heavy)

%define build_h323	0
%{?_without_h323:	%global build_h323 0}
%{?_with_h323:		%global build_h323 1}

# not compatible >=kernel-2.6.25 Using instead asterisk-chan_lcr
%define build_misdn	0
%{?_without_misdn:	%global build_misdn 0}
%{?_with_misdn:		%global build_misdn 1}

%define build_odbc	1
%{?_without_odbc:	%global build_odbc 0}
%{?_with_odbc:		%global build_odbc 1}

%define build_radius	1
%{?_without_radius:	%global build_radius 0}
%{?_with_radius:	%global build_radius 1}

%define build_tds	1
%{?_without_tds:	%global build_tds 0}
%{?_with_tds:		%global build_tds 1}

# SIP over TCP / TLS support: http://bugs.digium.com/view.php?id=4903
%define build_tcp	0
%{?_without_tcp:	%global build_tcp 0}
%{?_with_tcp:		%global build_tcp 1}

# this takes quite some time...
%define build_docs	0
%{?_without_docs:	%global build_docs 0}
%{?_with_docs:		%global build_docs 1}

%define build_imap	0
%{?_without_imap:	%global build_imap 0}
%{?_with_imap:		%global build_imap 1}

%define build_dahdi	1
%define build_osp	0
%define build_pri	1

%define build_ffmpeg	1
%{?_without_ffmpeg:	%global build_ffmpeg 0}
%{?_with_ffmpeg:	%global build_ffmpeg 1}

%if %mdkversion < 200900
%define build_dahdi	0
%define build_osp	0
%define build_pri	0
%endif
%define	astvardir	/var/lib/asterisk

Summary:	Asterisk PBX
Name:		asterisk16
Version:	%{version}
Release:	%{release}
License:	GPL
Group:		System/Servers
URL:		http://www.asterisk.org/
#Source0:	http://www.asterisk.org/html/downloads/%{name}-%{version}.tar.bz2
Source0:	%{name}-%{version}.tar.gz
Source1:	asterisk.init
Source2:	asterisk.sysconfig
Source3:	http://www.asteriskdocs.org/modules/tinycontent/content/docbook/current/AsteriskDocs-html.tar.bz2
Provides:	asterisk
Obsoletes:	asterisk
Requires(pre): rpm-helper
Requires(postun): rpm-helper
Requires(post): rpm-helper
Requires(preun): rpm-helper
Requires:	mpg123
Requires:	asterisk-core-sounds
Requires:	asterisk-core-moh
BuildRequires:	alsa-lib-devel
BuildRequires:	ast_menuselect
BuildRequires:	autoconf >= 1:2.60
BuildRequires:	automake1.9 >= 1.9.6
BuildRequires:	bison
BuildRequires:	bluez-devel
BuildRequires:	curl-devel
%if %{build_dahdi}
BuildRequires:	dahdi-devel >= 2.0.0
%endif
BuildRequires:	ffmpeg-devel
BuildRequires:	flex
#BuildRequires:	freetds-devel >= 0.64
BuildRequires:	gmime-devel
BuildRequires:	gsm-devel
BuildRequires:	gtk2-devel
BuildRequires:	jackit-devel
BuildRequires:	krb5-devel
BuildRequires:	libcap-devel
BuildRequires:	libcurl-devel
BuildRequires:	edit-devel
BuildRequires:	libgcrypt-devel
BuildRequires:	libgnutls-devel
BuildRequires:	libgpg-error-devel
BuildRequires:	libgsm-devel
BuildRequires:	%mklibname hoard
BuildRequires:	libidn-devel
BuildRequires:	libiksemel-devel
BuildRequires:	libilbc-devel
BuildRequires:	libjack-devel
BuildRequires:	libnbs-devel
BuildRequires:	libncurses-devel
BuildRequires:	libogg-devel
%if %{build_osp}
BuildRequires:	libosp-devel >= 3.5
%endif
BuildRequires:	libpopt-devel
%if %{build_pri}
BuildRequires:	libpri-devel >= 1.4.8
BuildRequires:	libss7-devel >= 1.0.2
%endif
BuildRequires:	libspeex-devel
BuildRequires:	libtermcap-devel
BuildRequires:	libtiff-devel
BuildRequires:	libtonezone-devel >= 1.4.0
BuildRequires:	libtool
BuildRequires:	libtool-devel
BuildRequires:	libusb-devel
BuildRequires:	libvorbis-devel
BuildRequires:	libzap-devel >= 1.0.1
BuildRequires:	lm_sensors-devel
BuildRequires:	lpc10-devel
BuildRequires:	lua-devel
BuildRequires:	newt-devel
BuildRequires:	openais-devel
BuildRequires:	openldap-devel
BuildRequires:	oggvorbis-devel
BuildRequires:	openssl-devel
BuildRequires:	pam-devel
BuildRequires:	perl-devel
BuildRequires:	portaudio-devel >= 19
BuildRequires:	postgresql-devel
BuildRequires:	radiusclient-ng-devel
BuildRequires:	resample-devel
BuildRequires:	SDL_image-devel
BuildRequires:	spandsp-devel
BuildRequires:	speex-devel
BuildRequires:	sqlite3-devel
BuildRequires:	tcp_wrappers-devel
BuildRequires:	termcap-devel
BuildRequires:	tiff-devel
BuildRequires:	sqlite-devel
#BuildRequires:	swig-devel
BuildRequires:	wget
%if %{build_imap}
%if %mdkversion < 200900
BuildRequires: imap-devel
%else
BuildRequires:	c-client-devel
%endif
%endif
%if %{build_misdn}
BuildRequires:	libmisdn-devel >= 1:3.4
BuildRequires:	isdn4k-utils-devel
%endif
%if %{build_docs}
BuildRequires:	doxygen
BuildRequires:	graphviz
%endif
%if %{build_h323}
BuildRequires:	ooh323c-devel
BuildRequires:	openh323-devel >= 1.15.3
BuildRequires:	pwlib-devel
%endif
BuildRoot:	%{_tmppath}/%{name}-%{version}-root

%description
Asterisk is an Open Source PBX and telephony development platform that
can both replace a conventional PBX and act as a platform for developing
custom telephony applications for delivering dynamic content over a
telephone similarly to how one can deliver dynamic content through a
web browser using CGI and a web server.

Asterisk talks to a variety of telephony hardware including BRI, PRI, 
POTS, and IP telephony clients using the Inter-Asterisk eXchange
protocol (e.g. gnophone or miniphone).  For more information and a
current list of supported hardware, see www.asterisk.org.

%package	firmware
Summary:	Firmware for the Digium S101I (IAXy)
Group:		System/Servers
License:	Redistributable, no modification permitted
Requires:	asterisk = %{version}-%{release}

%description	firmware
Firmware for the Digium S101I (IAXy).

%package	devel
Summary:	Header files for building Asterisk modules
Group:		Development/C

%description	devel
This package contains the development header files that are needed
to compile 3rd party modules.

%package	docs
Summary:	The Hitchhiker's Guide to Asterisk
Group:		Books/Howtos

%description	docs
The Hitchhiker's Guide to Asterisk

%package	plugins-ais
Summary:	Modules for Asterisk that use OpenAIS
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-ais
Modules for Asterisk that use OpenAIS.

%package	plugins-alsa
Summary:	Modules for Asterisk that use Alsa sound drivers
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-alsa
Modules for Asterisk that use Alsa sound drivers.

%package	plugins-curl
Summary:	Modules for Asterisk that use cURL
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-curl
Modules for Asterisk that use cURL.

%if %{build_dahdi}
%package	plugins-dahdi
Summary:	Modules for Asterisk that use DAHDI
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	dahdi-tools >= 2.0.0

%description	plugins-dahdi
Modules for Asterisk that use DAHDI.
%endif

%package	plugins-fax
Summary:	FAX plugins for Asterisk
Group:		System/Servers
Requires: %{name} = %{version}-%{release}

%description	plugins-fax
This package contains FAX plugins for Asterisk:

* Trivial FAX Receive Application
* Trivial FAX Transmit Application
* Assign entered string to a given variable

%package	plugins-festival
Summary:	Festival application for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	festival

%description	plugins-festival
Application for the Asterisk PBX that uses Festival to convert text to speech.

%package	plugins-ices
Summary:	Stream audio from Asterisk to an IceCast server
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	ices

%description	plugins-ices
Stream audio from Asterisk to an IceCast server.

%package	plugins-jabber
Summary:	Jabber support for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-jabber
This package contains Jabber support for Asterisk.

%package	plugins-jack
Summary:	JACK resources for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-jack
JACK resources for Asterisk.

%package	plugins-lua
Summary:	Lua resources for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-lua
Lua resources for Asterisk.

%package	plugins-ldap
Summary:	LDAP resources for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-ldap
LDAP resources for Asterisk.

%package	plugins-minivm
Summary:	MiniVM applicaton for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-minivm
MiniVM application for Asterisk.

%if %{build_misdn}
%package	chan_misdn
Summary:	This module adds mISDN support to the Asterisk PBX
Group:		System/Servers
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description	chan_misdn
This module adds mISDN support to the Asterisk PBX. mISDN is the
(experimental) new ISDN4Linux stack, which adds support for
driving cards in NT mode and thus connecting an ISDN phone to your
computer.
%endif

%if %{build_odbc}
%package	plugins-odbc
Summary:	ODBC plugins for Asterisk
Group:		System/Servers
BuildRequires:	libunixODBC-devel
BuildRequires:	libtool-devel
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description	plugins-odbc
This package contains ODBC plugins for Asterisk:

* ODBC Configuration
* Call Detail Recording for ODBC
* ODBC resource manager
%endif

%package	plugins-oss
Summary:	Modules for Asterisk that use OSS sound drivers
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-oss
Modules for Asterisk that use OSS sound drivers.

%package	plugins-portaudio
Summary:	Modules for Asterisk that use the portaudio library
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-portaudio
Modules for Asterisk that use the portaudio library.


%package	plugins-pgsql
Summary:	PostgreSQL plugins for Asterisk
Group:		System/Servers
BuildRequires:	postgresql-devel
BuildRequires:	zlib-devel
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description	plugins-pgsql
This package contains PostgreSQL plugins for Asterisk:

* Simple PostgreSQL Interface
* Call Detail Recording for PostgreSQL


%if %{build_radius}
%package	plugins-radius
Summary:	Radiusclient plugins for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-radius
This package contains Radiusclient plugins for Asterisk.
%endif

%package	plugins-skinny
Summary:	Modules for Asterisk that support the SCCP/Skinny protocol
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-skinny
Modules for Asterisk that support the SCCP/Skinny protocol.

%package	plugins-snmp
Summary:	Brief SNMP Agent / SubAgent support for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	net-snmp

%description	plugins-snmp
This package contains brief SNMP Agent / SubAgent support for Asterisk.

%package	plugins-speex
Summary:	SPEEX plugins for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-speex
This package contains SPEEX plugins for Asterisk.

%package	plugins-sqlite
Summary:	SQLite plugins for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-sqlite
This package contains SQLite plugins for Asterisk.

%if %{build_tds}
%package	plugins-tds
Summary:	FreeTDS plugins for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-tds
This package contains FreeTDS plugins for Asterisk.
%endif

%if %{build_osp}
%package	plugins-osp
Summary:	Open Settlement Protocol for Asterisk
Group:		System/Servers

%description	plugins-osp
This package contains OSP (Open Settlement Protocol) support for Asterisk.
%endif

%package	plugins-unistim
Summary:	Unistim channel for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-unistim
Unistim channel for Asterisk.

%package	plugins-usbradio
Summary:	USB radio channel for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}

%description	plugins-usbradio
USB radio channel for Asterisk.

%package	plugins-voicemail
Summary:	Common Voicemail Modules for Asterisk
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	asterisk-plugins-voicemail-implementation = %{version}-%{release}
Requires:	sox
Requires:	sendmail-command

%description	plugins-voicemail
Common Voicemail Modules for Asterisk.

%if %{build_imap}
%package	plugins-voicemail-imap
Summary:	Store voicemail on an IMAP server
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	asterisk-plugins-voicemail = %{version}-%{release}
Provides:	asterisk-plugins-voicemail-implementation = %{version}-%{release}

%description	plugins-voicemail-imap
Voicemail implementation for Asterisk that stores voicemail on an IMAP
server.
%endif

%if %{build_odbc}
%package	plugins-voicemail-odbc
Summary:	Store voicemail in a database using ODBC
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	asterisk-plugins-voicemail = %{version}-%{release}
Provides:	asterisk-plugins-voicemail-implementation = %{version}-%{release}

%description	plugins-voicemail-odbc
Voicemail implementation for Asterisk that uses ODBC to store
voicemail in a database.
%endif

%package	plugins-voicemail-plain
Summary:	Store voicemail on the local filesystem
Group:		System/Servers
Requires:	asterisk = %{version}-%{release}
Requires:	asterisk-plugins-voicemail = %{version}-%{release}
Provides:	asterisk-plugins-voicemail-implementation = %{version}-%{release}

%description	plugins-voicemail-plain
Voicemail implementation for Asterisk that stores voicemail on the
local filesystem.

%package        tests
Summary:        Testing utilities for Asterisk
Group:          System/Servers
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description    tests
This package contains a couple of testing utilities:

* refcounter : finds if asterisk objects are properly referenced
* tests_dllinklists : performs checks on double-linked lists upon
     asterisk startup

%prep
%git_get_source
%setup -q -a3
%git_gen_changelog -n 100

find . -type d -perm 0700 -exec chmod 755 {} \;
find . -type d -perm 0555 -exec chmod 755 {} \;
find . -type f -perm 0555 -exec chmod 755 {} \;
find . -type f -perm 0444 -exec chmod 644 {} \;
		
for i in `find . -type d -name CVS` `find . -type f -name .cvs\*` `find . -type f -name .#\*`; do
    if [ -e "$i" ]; then rm -rf $i; fi >&/dev/null
done

cat %{SOURCE1} > asterisk.init
cat %{SOURCE2} > asterisk.sysconfig

%if %mdkversion < 200900
cp contrib/mandriva/menuselect.makeopts.2008 ./menuselect.makeopts
%else
cp contrib/mandriva/menuselect.makeopts ./
%endif

chmod -x contrib/scripts/dbsep.cgi

# lib64 fix
find -name "Makefile" | xargs perl -pi -e "s|/usr/lib|%{_libdir}|g"
perl -pi -e "s|/lib\b|/%{_lib}|g" configure*

# fix one convenient softlink
pushd docs-html
    ln -s book1.html index.html
popd

%build
rm -f configure
sh ./bootstrap.sh

echo "%{version}-%{release}" > .version

export CFLAGS="%{optflags} `gmime-config --cflags`"

%configure \
    --localstatedir=/var \
    --with-asound=%{_prefix} \
    --with-execinfo=%{_prefix} \
    --with-cap=%{_prefix} \
    --with-curl=%{_prefix} \
    --with-curses=%{_prefix} \
    --with-crypto=%{_prefix} \
%if %{build_dahdi}
    --with-dahdi=%{_prefix} \
%else
    --with-dahdi=no \
%endif
%if %{build_ffmpeg}
    --with-avcodec=%{_prefix} \
%endif
    --with-gsm=%{_prefix} \
    --without-gtk \
    --with-gtk2=%{_prefix} \
    --with-hoard=%{_prefix} \
    --with-iconv=%{_prefix} \
    --with-iksemel=%{_prefix} \
%if %{build_imap}
    --with-imap=system \
%endif
    --with-jack=%{_prefix} \
    --with-ldap=%{_prefix} \
    --with-libedit=%{_prefix} \
    --with-ltdl=%{_prefix} \
    --with-lua=%{_prefix} \
%if %{build_misdn}
    --with-isdnnet=%{_prefix} \
    --with-misdn=%{_prefix} \
    --with-suppserv=%{_prefix} \
%else
    --without-isdnnet \
    --without-misdn \
    --without-suppserv \
%endif
    --with-nbs=%{_prefix} \
    --with-ncurses=%{_prefix} \
    --with-netsnmp=%{_prefix} \
    --with-newt=%{_prefix} \
%if %{build_odbc}
    --with-odbc=%{_prefix} \
%else
    --without-odbc \
%endif
    --with-ogg=%{_prefix} \
%if %{build_osp}
    --with-osptk=%{_prefix} \
%else
    --with-osptk=no \
%endif
    --with-postgres=%{_prefix} \
    --with-popt=%{_prefix} \
    --with-portaudio=%{_prefix} \
%if %{build_pri}
    --with-pri=%{_prefix} \
    --with-ss7=%{_prefix} \
%endif
    --with-resample=%{_prefix} \
    --with-spandsp=%{_prefix} \
%if %{build_h323}
    --with-pwlib=%{_prefix} \
    --with-h323=%{_prefix} \
%else
    --without-pwlib \
    --without-h323 \
%endif
    --with-radius=%{_prefix} \
    --with-sdl=%{_prefix} \
    --with-SDL_image=%{_prefix} \
    --with-openais=%{_prefix} \
    --with-speexdsp=%{_prefix} \
    --without-sqlite \
    --with-sqlite3=%{_prefix} \
    --with-ssl=%{_prefix} \
    --with-tds=%{_prefix} \
    --with-termcap=%{_prefix} \
    --without-tinfo \
    --with-tonezone=%{_prefix} \
    --with-usb=%{_prefix} \
    --with-vorbis=%{_prefix} \
    --without-vpb \
    --with-x11=%{_prefix} \
    --with-z=%{_prefix}

#pushd menuselect
#	# Just configure for the host system
#	./configure --prefix=%{_prefix}
#popd

%make -j1 cleantest

export ASTCFLAGS="%{optflags}"
%make ASTVARRUNDIR=/var/run/asterisk

%if %{build_docs}
%make progdocs
%endif

%install
[ -n "%{buildroot}" -a "%{buildroot}" != / ] && rm -rf %{buildroot}
install -d %{buildroot}%{astvardir}
install -d %{buildroot}/var/www/{html,cgi-bin}
install -d %{buildroot}/var/run/asterisk
install -d %{buildroot}/var/log/asterisk
install -d %{buildroot}/var/log/asterisk/cdr-csv
install -d %{buildroot}/var/spool/asterisk
install -d %{buildroot}/var/spool/asterisk/outgoing

# ASTSBINDIR="%{_sbindir}" HTTPDIR="/var/www"
%makeinstall_std \
	 samples webvmail adsi

# don't fiddle with the initscript!
export DONT_GPRINTIFY=1


# install init scrips
install -d %{buildroot}%{_initrddir}
install -m0755 asterisk.init %{buildroot}%{_initrddir}/asterisk

# install sysconfig file
install -d %{buildroot}%{_sysconfdir}/sysconfig
install -m0644 asterisk.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/asterisk

# fix logrotation
install -d %{buildroot}%{_sysconfdir}/logrotate.d
cat > %{buildroot}%{_sysconfdir}/logrotate.d/asterisk << EOF
/var/log/asterisk/console /var/log/asterisk/debug /var/log/asterisk/messages /var/log/asterisk/full /var/log/asterisk/queue_log /var/log/asterisk/event_log /var/log/asterisk/cdr-custom/*.csv /var/log/asterisk/cdr-csv/*.csv {
    weekly
    size 10M
    rotate 5
    copytruncate
    compress
    notifempty
    missingok
    postrotate
    %{_sbindir}/asterisk -rx 'reload'
    endscript
}
EOF

touch %{name}-devel.filelist
%if %{build_docs}
    find doc/api/html -type f | sed 's/^/%doc /' | grep -v '\./%{name}-devel.filelist' > %{name}-devel.filelist
%endif

# fix ghost files
#touch	%{buildroot}%{astvardir}/astdb
touch	%{buildroot}%{astvardir}/astdb
touch	%{buildroot}/var/log/asterisk/console
touch	%{buildroot}/var/log/asterisk/debug
touch	%{buildroot}/var/log/asterisk/messages
touch	%{buildroot}/var/log/asterisk/queue_log
touch	%{buildroot}/var/log/asterisk/event_log
touch	%{buildroot}/var/log/asterisk/cdr-csv/Master.csv
touch	%{buildroot}/var/log/asterisk/h323_log

# Fix incorrect path in /etc/asterisk/asterisk.conf
perl -pi -e "s|astrundir => /var/run|astrundir => /var/run/asterisk|g" %{buildroot}/%{_sysconfdir}/asterisk/asterisk.conf
perl -pi -e "s|^libdir=.*|libdir=%{_libdir}|g" %{buildroot}%{_libdir}/pkgconfig/asterisk.pc
perl -pi -e "s|^varrundir=.*|varrundir=/var/run/asterisk|g" %{buildroot}%{_libdir}/pkgconfig/asterisk.pc

# TODO
# Add directory for ssl certs
#mkdir -p %{buildroot}%{_sysconfdir}/ssl/%{name}

# Remove unpackages files
rm -rf %{buildroot}%{astvardir}/moh/.asterisk-moh-freeplay-wav

# use the stand alone asterisk-core-sounds package instead
rm -rf %{buildroot}%{astvardir}/sounds

%pre
%_pre_useradd asterisk %{astvardir} /bin/sh

%post
%create_ghostfile /var/log/asterisk/console asterisk asterisk 640
%create_ghostfile /var/log/asterisk/debug asterisk asterisk 640
%create_ghostfile /var/log/asterisk/messages asterisk asterisk 640
%create_ghostfile /var/log/asterisk/queue_log asterisk asterisk 640
%create_ghostfile /var/log/asterisk/event_log asterisk asterisk 640
%create_ghostfile /var/log/asterisk/cdr-csv/Master.csv asterisk asterisk 640
%create_ghostfile /var/log/asterisk/h323_log asterisk asterisk 640
echo "Adding setuid root to /usr/bin/mpg123, needed for MOH"
chmod u+s %{_bindir}/mpg123
%_post_service asterisk

%preun
if [ "$1" = 0 ]; then
    echo "Removing setuid root from /usr/bin/mpg123"
    chmod u-s %{_bindir}/mpg123
fi

# TODO
# generate the ldap.pem cert here instead of the initscript
#"/etc/ssl/asterisk/trustcerts.pem"
#"/etc/ssl/asterisk/trustdir"
#"/etc/ssl/asterisk/servercert.pem"
#"/etc/ssl/asterisk/serverkey.pem"
#"/etc/ssl/asterisk/dh512.pem"
#"/etc/ssl/asterisk/dh1024.pem"
#if [ ! -e %{_sysconfdir}/ssl/%{name}/ldap.pem ] ; then
#	if [ -x %{_datadir}/%{name}/gencert.sh ] ; then
#		echo "Generating self-signed certificate..."
#		pushd %{_sysconfdir}/ssl/%{name}/ > /dev/null
#		yes ""|%{_datadir}/%{name}/gencert.sh >/dev/null 2>&1
#		chmod 640 ldap.pem
#		chown root:ldap ldap.pem
#		popd > /dev/null
#	fi
#	echo "To generate a self-signed certificate, you can use the utility"
#	echo "%{_datadir}/%{name}/gencert.sh..."
#fi

%_preun_service asterisk

%postun
%_postun_userdel asterisk

%if %{build_dahdi}
%pre plugins-dahdi
%{_sbindir}/usermod -a -G dahdi asterisk
%endif

%if %{build_misdn}
%pre plugins-misdn
%{_sbindir}/usermod -a -G misdn asterisk
%endif

%clean
[ -n "%{buildroot}" -a "%{buildroot}" != / ] && rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc BUGS CHANGES CREDITS LICENSE README* apps/rpt_flow.pdf
%doc doc/*.txt contrib/init.d/rc.mandriva* contrib/asterisk-ices.xml
%doc contrib/scripts contrib/i18n.testsuite.conf contrib/README.festival
%attr(0755,root,root)					%{_initrddir}/asterisk
%attr(0644,root,root) %config(noreplace)		%{_sysconfdir}/logrotate.d/asterisk
%attr(0750,asterisk,asterisk) %dir			%{_sysconfdir}/asterisk
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/*.adsi
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/*.conf
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/extensions.ael
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/extensions.lua
%attr(0644,root,root) %config(noreplace)		%{_sysconfdir}/sysconfig/asterisk
# TODO
#attr(0750,root,asterisk) %dir				%{_sysconfdir}/ssl/asterisk

# these are packaged as sub packages below
%if %{build_misdn}
%exclude						%{_sysconfdir}/asterisk/misdn.conf
%endif
%if %{build_odbc}
%exclude						%{_sysconfdir}/asterisk/*_odbc.conf
%endif
%if %{build_tds}
%exclude						%{_sysconfdir}/asterisk/*tds*.conf
%endif
%exclude						%{_sysconfdir}/asterisk/cdr_pgsql.conf
%exclude						%{_sysconfdir}/asterisk/gtalk.conf
%exclude						%{_sysconfdir}/asterisk/jabber.conf
%exclude						%{_sysconfdir}/asterisk/osp.conf
%exclude						%{_sysconfdir}/asterisk/res_snmp.conf
%exclude						%{_sysconfdir}/asterisk/*sql*.conf

%attr(0755,root,root)					%{_sbindir}/aelparse
%attr(0755,root,root)					%{_sbindir}/asterisk
%attr(0755,root,root)					%{_sbindir}/astgenkey
%attr(0755,root,root)					%{_sbindir}/astman
%attr(0755,root,root)					%{_sbindir}/autosupport
%attr(0755,root,root)					%{_sbindir}/muted
%attr(0755,root,root)					%{_sbindir}/rasterisk
%attr(0755,root,root)					%{_sbindir}/safe_asterisk
%attr(0755,root,root)					%{_sbindir}/smsq
%attr(0755,root,root)					%{_sbindir}/stereorize
%attr(0755,root,root)					%{_sbindir}/streamplayer
%attr(0755,root,root)					%{_sbindir}/astcanary
# %attr(0755,root,root)					%{_sbindir}/check_expr
%attr(0755,root,root)					%{_sbindir}/conf2ael
%attr(0755,root,root)					%{_sbindir}/hashtest
%attr(0755,root,root)					%{_sbindir}/hashtest2
%exclude						%{_sbindir}/refcounter

%attr(0755,root,root)		%dir			%{_libdir}/asterisk
%attr(0755,root,root)		%dir			%{_libdir}/asterisk/modules
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_adsiprog.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_alarmreceiver.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_amd.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_authenticate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_cdr.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_chanisavail.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_channelredirect.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_chanspy.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_controlplayback.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_confbridge.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_db.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_dial.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_dictate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_directed_pickup.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_disa.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_dumpchan.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_echo.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_exec.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_externalivr.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_followme.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_forkcdr.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_getcpeid.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_image.so
# attr(0755,root,root) %{_libdir}/asterisk/modules/app_ivrdemo.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_macro.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_milliwatt.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_mixmonitor.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_morsecode.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_mp3.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_nbscat.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_parkandannounce.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_playback.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_privacy.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_queue.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_readexten.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_readfile.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_read.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_record.so
%if %mdkversion >= 200900
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_rpt.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_timing_timerfd.so
%endif
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_sayunixtime.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_senddtmf.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_sendtext.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_setcallerid.so
# attr(0755,root,root) %{_libdir}/asterisk/modules/app_skel.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_sms.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_softhangup.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_speech_utils.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_stack.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_system.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_talkdetect.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_test.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_transfer.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_url.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_userevent.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_waitforring.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_waitforsilence.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_waituntil.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_verbose.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_while.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_zapateller.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/bridge*.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_csv.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_custom.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_manager.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_agent.so
#%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_features.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_iax2.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_local.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_mgcp.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_nbs.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_phone.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_sip.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_adpcm.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_alaw.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_a_mu.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_g722.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_g726.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_gsm.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_ilbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_lpc10.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_resample.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_ulaw.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_g723.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_g726.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_g729.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_gsm.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_h263.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_h264.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_ilbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_jpeg.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_ogg_vorbis.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_pcm.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_sln16.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_sln.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_siren14.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_siren7.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_aes.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_connectedline.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_wav_gsm.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_wav.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/format_vox.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_audiohookinherit.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_base64.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_blacklist.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_callerid.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_cdr.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_channel.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_config.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_cut.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_db.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_devstate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_dialgroup.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_dialplan.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_enum.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_env.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_extstate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_global.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_groupcount.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_iconv.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_lock.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_logic.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_math.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_md5.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_module.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_rand.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_realtime.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_sha1.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_shell.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_strings.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_sysinfo.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_timeout.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_uri.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_version.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_volume.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_ael.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_config.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_dundi.so
# attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_gtkconsole.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_loopback.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_realtime.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_spool.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_adsi.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_ael_share.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_agi.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_clioriginate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_convert.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_crypto.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_http_post.so
# attr(0755,root,root) %{_libdir}/asterisk/modules/res_indications.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_limit.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_monitor.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_musiconhold.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_phoneprov.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_realtime.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_smdi.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_speech.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_timing_pthread.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/test_dlinklists.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/test_heap.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_redirecting.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_sprintf.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_clialiases.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_rtp_asterisk.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_originate.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_playtones.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_bridge.so

%if 0
%attr(0755,root,root) %{_libdir}/asterisk/modules/test_skel.so
%endif

#attr(0755,asterisk,asterisk)	%dir			%{astvardir}
%attr(0755,root,root)		%dir			%{astvardir}/agi-bin
%attr(0755,root,root)					%{astvardir}/agi-bin/*
%ghost							%{astvardir}/astdb
%attr(0755,root,root)		%dir			%{astvardir}/firmware
%attr(0755,root,root)		%dir			%{astvardir}/firmware/iax
%attr(0755,root,root)					%{astvardir}/firmware/iax/*.bin
%attr(0755,root,root)		%dir			%{astvardir}/images
%attr(0644,root,root)					%{astvardir}/images/*.jpg
%attr(0755,root,root)		%dir			%{astvardir}/keys
%attr(0644,root,root)					%{astvardir}/keys/*.pub
%attr(0755,root,root)		%dir			%{astvardir}/moh
%if 0
%attr(0644,root,root)					%{astvardir}/moh/*.wav
%endif
# %doc							%{astvardir}/moh/LICENSE-asterisk-moh-freeplay-wav
#attr(0755,root,root)		%dir			%{astvardir}/mohmp3
#attr(0644,root,root)					%{astvardir}/mohmp3/*.mp3
%attr(0755,root,root)		%dir			%{astvardir}/static-http
%attr(0644,root,root)					%{astvardir}/static-http/*

%attr(0750,asterisk,asterisk)	%dir			/var/log/asterisk
%attr(0750,asterisk,asterisk)	%dir			/var/log/asterisk/cdr-csv
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/cdr-csv/Master.csv
%attr(0750,asterisk,asterisk)	%dir			/var/log/asterisk/cdr-custom
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/console
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/debug
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/event_log
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/h323_log
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/messages
%attr(644,asterisk,asterisk)	%ghost			/var/log/asterisk/queue_log
#attr(0750,asterisk,asterisk)	%dir			/var/log/asterisk/testreports

%attr(0750,asterisk,asterisk)	%dir			/var/run/asterisk

%attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/dictate
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/meetme
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/monitor
%attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/outgoing
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/system
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/tmp
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/vm
%attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/voicemail
%attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/voicemail/default
%attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/voicemail/default/1234
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/voicemail/default/1234/INBOX
#%attr(0644,asterisk,asterisk)				/var/spool/asterisk/voicemail/default/1234/busy.gsm
#%attr(0644,asterisk,asterisk)				/var/spool/asterisk/voicemail/default/1234/unavail.gsm
#attr(0750,asterisk,asterisk)	%dir			/var/spool/asterisk/voicemail/voicemail
%attr(0644,root,root)					%{astvardir}/phoneprov/*.cfg
%attr(0644,root,root)					%{astvardir}/phoneprov/*.xml

%attr(0755,asterisk,asterisk)	%dir			%{astvardir}/documentation
%attr(-,asterisk,asterisk)				%{astvardir}/documentation/*

							%{_mandir}/man8/asterisk.8*
							%{_mandir}/man8/astgenkey.8*
							%{_mandir}/man8/autosupport.8*
							%{_mandir}/man8/safe_asterisk.8*
%exclude /var/www/
%exclude %{_libdir}/asterisk/modules/app_flash.so
%exclude %{_libdir}/asterisk/modules/app_meetme.so
%exclude %{_libdir}/asterisk/modules/app_page.so
%exclude %{_libdir}/asterisk/modules/app_dahdibarge.so
%exclude %{_libdir}/asterisk/modules/app_dahdiras.so
# exclude %{_libdir}/asterisk/modules/app_dahdiscan.so
%exclude %{_libdir}/asterisk/modules/chan_dahdi.so
%exclude %{_libdir}/asterisk/modules/codec_dahdi.so
%exclude %{_libdir}/asterisk/modules/res_timing_dahdi.so


%files devel -f %{name}-devel.filelist
%defattr(-,root,root,-)
%doc doc/CODING-GUIDELINES doc/datastores.txt doc/modules.txt doc/valgrind.txt
%dir %{_includedir}/asterisk
%{_includedir}/asterisk.h
%{_includedir}/asterisk/*.h
%{_includedir}/asterisk/doxygen/*.h
%{_libdir}/pkgconfig/asterisk.pc

%files firmware
%defattr(-,root,root,-)
%attr(0750,asterisk,asterisk) /var/lib/asterisk/firmware

%if %mdkversion >= 200900
%files plugins-ais
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/ais.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_ais.so
%endif

%files plugins-alsa
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/alsa.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_alsa.so

%files plugins-curl
%defattr(-,root,root,-)
%doc contrib/scripts/dbsep.cgi
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/dbsep.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_curl.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_config_curl.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_curl.so

%if %{build_dahdi}
%files plugins-dahdi
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/meetme.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/chan_dahdi.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_flash.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_meetme.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_page.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_dahdibarge.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_dahdiras.so
#attr(0755,root,root) %{_libdir}/asterisk/modules/app_dahdiscan.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_dahdi.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_dahdi.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_timing_dahdi.so
%endif

%if %mdkversion >= 200900
%files plugins-fax
%defattr(-,root,root,-)
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_fax.so
%endif

%files plugins-festival
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/festival.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_festival.so

%files plugins-ices
%defattr(-,root,root,-)
%doc contrib/asterisk-ices.xml
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_ices.so

%files plugins-jabber
%defattr(-,root,root,-)
%doc doc/jabber.txt doc/jingle.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/gtalk.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/jabber.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/jingle.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_gtalk.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_jingle.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_jabber.so

%files plugins-jack
%defattr(-,root,root,-)
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_jack.so

%files plugins-lua
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/extensions.lua
%attr(0755,root,root) %{_libdir}/asterisk/modules/pbx_lua.so

%files plugins-ldap
%defattr(-,root,root,-)
%doc doc/ldap.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_ldap.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_config_ldap.so

%files plugins-minivm
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/extensions_minivm.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/minivm.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_minivm.so

%if %{build_misdn}
%files plugins-misdn
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/misdn.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_misdn.so
%endif

%if %{build_odbc}
%files plugins-odbc
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_adaptive_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/func_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_odbc.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_adaptive_odbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_odbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_odbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_config_odbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_odbc.so
%endif

%files plugins-oss
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/oss.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_oss.so

%if %{build_osp}
%files plugins-osp
%defattr(-,root,root)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/osp.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_osplookup.so
%endif

%files plugins-portaudio
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/console.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_console.so

%files plugins-pgsql
%defattr(-,root,root,-)
%doc contrib/scripts/realtime_pgsql.sql
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_pgsql.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_pgsql.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_pgsql.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_config_pgsql.so

%files plugins-radius
%defattr(-,root,root,-)
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_radius.so

%files plugins-skinny
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/skinny.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_skinny.so

%files plugins-snmp
%defattr(-,root,root,-)
%doc doc/asterisk-mib.txt
%doc doc/digium-mib.txt
%doc doc/snmp.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_snmp.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_snmp.so

%files plugins-sqlite
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_sqlite3_custom.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_sqlite.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_sqlite3_custom.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/res_config_sqlite.so

%files plugins-speex
%defattr(-,root,root,-)
%attr(0755,root,root) %{_libdir}/asterisk/modules/codec_speex.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_speex.so

%if %{build_tds}
%files plugins-tds
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_tds.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/cdr_tds.so
%endif

%files plugins-unistim
%defattr(-,root,root,-)
%doc doc/unistim.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/unistim.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_unistim.so

%if 0
%files plugins-usbradio
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/usbradio.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/chan_usbradio.so
%endif

%files plugins-voicemail
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/voicemail.conf
%attr(0755,root,root) %{_libdir}/asterisk/modules/func_vmcount.so

%if %{build_imap}
#FIXME: find a better way to build dir_imap etc.
%files plugins-voicemail-imap
%defattr(-,root,root,-)
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_directory_imap.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_voicemail_imap.so
%endif

%if 0
# FIXME
%if %{build_odbc}
%files plugins-voicemail-odbc
%defattr(-,root,root,-)
%doc doc/voicemail_odbc_postgresql.txt
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_directory_odbc.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_voicemail_odbc.so
%endif
%endif

#FIXME!
%files plugins-voicemail-plain
%defattr(-,root,root,-)
# attr(0755,root,root) %{_libdir}/asterisk/modules/app_directory_plain.so
# attr(0755,root,root) %{_libdir}/asterisk/modules/app_voicemail_plain.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_directory.so
%attr(0755,root,root) %{_libdir}/asterisk/modules/app_voicemail.so


%files docs
%defattr(-,root,root)
%doc	docs-html/*

%files tests
%attr(0755,root,root)  %{_libdir}/asterisk/modules/test_dlinklists.so
%attr(0755,root,root)  %{_libdir}/asterisk/modules/test_sched.so
%attr(0755,root,root)  %{_libdir}/asterisk/modules/test_logger.so
%attr(0755,root,root)  %{_libdir}/asterisk/modules/test_substitution.so
%attr(0755,root,root)  %{_sbindir}/refcounter

%changelog -f Changelog.git.txt

