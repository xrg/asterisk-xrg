%define git_repo asterisk

%define _requires_exceptions perl(Carp::Heavy)
%define sounds_core_version 1.5

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

%define build_tds	0
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
%define build_pri	0

%define build_ffmpeg	1
%{?_without_ffmpeg:	%global build_ffmpeg 0}
%{?_with_ffmpeg:	%global build_ffmpeg 1}

# Note: at Mandriva/Mageia there is no /var/lib64 ..
%define astvardir	/var/lib/asterisk
%define modulesdir	%{_libdir}/asterisk/modules
%define soundsdir %{astvardir}/sounds
%define mohdir %{astvardir}/moh

%if %{mgaversion} < 3
%define _tmpfilesdir /usr/lib/tmpfiles.d
%define _tmpfilescreate() /bin/systemd-tmpfiles --create \
%{nil}
%define systemd_required_version 44
%endif

%define version %git_get_ver

Summary:	Asterisk PBX
Name:		asterisk13
Version:	%version
Release:	%git_get_rel
License:	GPL
Group:		System/Servers
URL:		http://www.asterisk.org/
Source0:	%git_bs_source %{name}-%{version}.tar.gz
Source1:	%{name}-gitrpm.version
Source2:	%{name}-changelog.gitrpm.txt
Provides:	asterisk
Obsoletes:	asterisk
Obsoletes:	asterisk16
Requires(pre): rpm-helper
Requires(postun): rpm-helper
Requires(post): rpm-helper
Requires(preun): rpm-helper
Requires:	mpg123
Requires:	asterisk-sounds-core
Requires:	asterisk-moh
BuildRequires:	alsa-lib-devel
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
BuildRequires:  jansson-devel
BuildRequires:	libgcrypt-devel
BuildRequires:	libgnutls-devel
BuildRequires:	libgpg-error-devel
BuildRequires:	libgsm-devel
BuildRequires:	libidn-devel
BuildRequires:	libiksemel-devel
BuildRequires:	libilbc-devel
BuildRequires:	libjack-devel
%if %{_host_vendor} == mandriva
BuildRequires:	%mklibname hoard
BuildRequires:	libnbs-devel
%endif
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
# BuildRequires:	libzap-devel >= 1.0.1
BuildRequires:	lm_sensors-devel
BuildRequires:	lpc10-devel
BuildRequires:	lua-devel
BuildRequires:	newt-devel
BuildRequires:	oggvorbis-devel
BuildRequires:	openssl-devel
BuildRequires:	pam-devel
BuildRequires:	perl-devel
BuildRequires:	portaudio-devel >= 19
BuildRequires:	postgresql-devel
BuildRequires:	radiusclient-ng-devel
BuildRequires:	resample-devel
BuildRequires:	SDL_image-devel
BuildRequires:	spandsp-devel >= 0.0.6
BuildRequires:	speex-devel
BuildRequires:	sqlite3-devel
BuildRequires:	tcp_wrappers-devel
BuildRequires:	termcap-devel
BuildRequires:	tiff-devel
BuildRequires:	sqlite-devel
#BuildRequires:	swig-devel
BuildRequires:	wget
BuildRequires: neon-devel
BuildRequires: libical-devel
BuildRequires: libxml2-devel
BuildRequires: libuuid-devel
BuildRequires: net-snmp-devel
%if %{build_imap}

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
BuildArch:      noarch

%description	docs
The Hitchhiker's Guide to Asterisk

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

%package       plugins-jabber
Summary:       Jabber support for Asterisk
Group:         System/Servers
Requires:      asterisk = %{version}-%{release}

%description   plugins-jabber
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

%package        ari
Summary:        RESTful web API for Asterisk
Group:          System/Servers
Requires:       asterisk = %{version}-%{release}
BuildArch:      noarch

%description    ari
HTTP binding for the Stasis API


%package        plugins-ooh323
Summary:        Objective System's H323 for Asterisk
Group:          System/Servers
Requires:       asterisk = %{version}-%{release}

%description    plugins-ooh323
Objective System's H323 for Asterisk.

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

%package	plugins-mysql
Summary:	MySQL plugins for Asterisk
Group:		System/Servers
BuildRequires:	mysql-devel
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description	plugins-mysql
This package contains MySQL (Oracle) plugins for Asterisk


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

# if build sounds

%package sounds-core-en-alaw
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-en-alaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: a-Law

%package sounds-core-en-ulaw
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-en-ulaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: mu-Law

%package sounds-core-en-g729
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
# Provides:     asterisk-sounds-core # NOT to be used globaly

%description sounds-core-en-g729
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.729
If used, it avoids using the g729 codecs for these pre-recorded sounds.

%package sounds-core-en-sln16
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
# Provides:     asterisk-sounds-core # NOT to be used globaly

%description sounds-core-en-sln16
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: Signed Linear 16bit

%if 0
%package sounds-core-en-g722
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch

%description sounds-core-en-g722
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.722
%endif

%package sounds-core-en-wav
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-en-wav
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: WAV

%package sounds-core-en-gsm
Summary:        English sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-en-gsm
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: GSM

%if 0
%package sounds-core-fr-gsm
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-fr-gsm
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: GSM

%package sounds-core-fr-alaw
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-fr-alaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: a-Law

%package sounds-core-fr-ulaw
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-fr-ulaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: mu-Law

%package sounds-core-fr-g729
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
# Provides:     asterisk-sounds-core # NOT to be used globaly

%description sounds-core-fr-g729
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.729
If used, it avoids using the g729 codecs for these pre-recorded sounds.

%package sounds-core-fr-g722
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch


%description sounds-core-fr-g722
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.722

%package sounds-core-fr-wav
Summary:        French sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-fr-wav
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: WAV

%package sounds-core-es-gsm
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-es-gsm
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: GSM

%package sounds-core-es-alaw
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-es-alaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: a-Law

%package sounds-core-es-ulaw
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-es-ulaw
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: mu-Law

%package sounds-core-es-g729
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
# Provides:     asterisk-sounds-core # NOT to be used globaly

%description sounds-core-es-g729
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.729
If used, it avoids using the g729 codecs for these pre-recorded sounds.

%package sounds-core-es-g722
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch

%description sounds-core-es-g722
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: G.722

%package sounds-core-es-wav
Summary:        Spanish sound files for the Asterisk PBX and telephony application and toolkit
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-sounds-core

%description sounds-core-es-wav
This package contains freely usable recorded sounds that were meant to be used
with Asterisk in the following formats: WAV

%endif

%package moh-opsound-alaw
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-moh-core

%description moh-opsound-alaw
This package contains freely usable recorded music to be used with Asterisk, in
A-law format.

%package moh-opsound-ulaw
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-moh-core

%description moh-opsound-ulaw
This package contains freely usable recorded music to be used with Asterisk, in
mu-law format.

%package moh-opsound-g722
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch

%description moh-opsound-g722
This package contains freely usable recorded music to be used with Asterisk, in
g722 format.

%package moh-opsound-g729
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch

%description moh-opsound-g729
This package contains freely usable recorded music to be used with Asterisk, in
g729 format.

%package moh-opsound-wav
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-moh-core

%description moh-opsound-wav
This package contains freely usable recorded music to be used with Asterisk, in
wav format.

%package moh-opsound-gsm
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-moh-core

%description moh-opsound-gsm
This package contains freely usable recorded music to be used with Asterisk, in
GSM format.

%package moh-opsound-sln16
Summary:        Music on hold for the Asterisk PBX
Group:          System/Servers
BuildArch:      noarch
Provides:       asterisk-moh-core

%description moh-opsound-sln16
This package contains freely usable recorded music to be used with Asterisk, in
Signed-Linear-16bit format.

# endif sounds

%package        tests
Summary:        Testing utilities for Asterisk
Group:          System/Servers
Requires(post): %{name} = %{version}
Requires(preun): %{name} = %{version}

%description    tests
This package contains a couple of testing utilities:

        check_expr[2]


%prep
%git_get_source
%setup -q

find . -type d -perm 0700 -exec chmod 755 {} \;
find . -type d -perm 0555 -exec chmod 755 {} \;
find . -type f -perm 0555 -exec chmod 755 {} \;
find . -type f -perm 0444 -exec chmod 644 {} \;
		
for i in `find . -type d -name CVS` `find . -type f -name .cvs\*` `find . -type f -name .#\*`; do
    if [ -e "$i" ]; then rm -rf $i; fi >&/dev/null
done

chmod -x contrib/scripts/dbsep.cgi

# lib64 fix
find -name "Makefile" | xargs perl -pi -e "s|/usr/lib|%{_libdir}|g"
perl -pi -e "s|/lib\b|/%{_lib}|g" configure*

%build
rm -f configure
sh ./bootstrap.sh

echo "%{version}-%{release}" > .version

export CFLAGS="%{optflags} `gmime-config --cflags`"

%configure \
    --localstatedir=%{_var} \
    --with-asound=%{_prefix} \
    --with-execinfo=%{_prefix} \
    --with-sounds-cache=%{_sourcedir} \
    --with-cap=%{_prefix} \
    --with-libcurl=%{_prefix} \
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
%if %{_host_vendor} == mandriva
    --with-hoard=%{_prefix} \
    --with-nbs=%{_prefix} \
%endif
    --with-ncurses=%{_prefix} \
    --with-netsnmp=%{_prefix} \
    --with-newt=%{_prefix} \
%if %{build_odbc}
    --with-unixodbc=%{_prefix} \
%else
    --without-unixodbc \
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
    --with-SDL_image=%{_prefix} \
    --with-speexdsp=%{_prefix} \
    --with-sqlite=%{_prefix} \
    --with-sqlite3=%{_prefix} \
    --with-ssl=%{_prefix} \
    --with-termcap=%{_prefix} \
    --without-tinfo \
    --with-tonezone=%{_prefix} \
    --with-usb=%{_prefix} \
    --with-vorbis=%{_prefix} \
    --without-vpb \
    --with-x11=%{_prefix} \
    --with-z=%{_prefix} \
%if %{build_tds}
    --with-tds_mssql=%{_prefix} \
%else
    --without-tds_mssql
%endif

export ASTCFLAGS="%{optflags}"

%make menuselect.makeopts
# Then, turn on some non-default options:
menuselect/menuselect --enable chan_mobile --enable chan_ooh323 --enable res_config_mysql \
        --enable aelparse --enable astman --enable check_expr --enable check_expr2 --enable conf2ael \
        --enable muted --enable smsq --enable stereorize --enable streamplayer \
        --enable CORE-SOUNDS-EN-ALAW --enable CORE-SOUNDS-EN-ULAW --enable CORE-SOUNDS-EN-GSM \
        --enable CORE-SOUNDS-EN-WAV --enable CORE-SOUNDS-EN-G729 --enable CORE-SOUNDS-EN-SLN16 \
        --enable MOH-OPSOUND-ULAW --enable MOH-OPSOUND-ALAW \
        --enable MOH-OPSOUND-G729 --enable MOH-OPSOUND-SLN16 \
        

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

%if %{mgaversion} >=3
install -D -p -m 0644 contrib/mandriva/asterisk.service %{buildroot}%{_unitdir}/%{name}.service
rm -f %{buildroot}%{_sbindir}/safe_asterisk
install -D -p -m 0644 contrib/mandriva/asterisk-tmpfiles %{buildroot}%{_tmpfilesdir}/%{name}.conf
%else
# install init scrips
install -d %{buildroot}%{_initrddir}
install -m0755 contrib/mandriva/asterisk.init %{buildroot}%{_initrddir}/asterisk
%endif

# install sysconfig file
install -d %{buildroot}%{_sysconfdir}/sysconfig
install -m0644 contrib/mandriva/asterisk.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/asterisk

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

# move /var/lib64
if [ -d %{buildroot}/var/lib64 ] ; then
    mv %{buildroot}/var/lib64/* %{buildroot}/var/lib/
fi

# fix ghost files
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
%if %{_arch} == "x86_64"
sed -i 's|/var/lib64/|/var/lib/|' %{buildroot}/%{_sysconfdir}/asterisk/asterisk.conf
%endif
perl -pi -e "s|^libdir=.*|libdir=%{_libdir}|g" %{buildroot}%{_libdir}/pkgconfig/asterisk.pc
perl -pi -e "s|^varrundir=.*|varrundir=/var/run/asterisk|g" %{buildroot}%{_libdir}/pkgconfig/asterisk.pc


# TODO
# Add directory for ssl certs
#mkdir -p %{buildroot}%{_sysconfdir}/ssl/%{name}

# Remove unpackages files
rm -r %{buildroot}%{astvardir}/moh/.asterisk-moh-*
rm -r %{buildroot}%{astvardir}/sounds/*/.asterisk-core-sounds-*
rm -r %{buildroot}%{astvardir}/sounds/en/core-sounds-en.txt
#                                                          TODO: move to docs


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

%if %{mgaversion} >=3 
%_tmpfilescreate %{name}
%endif
%_post_service %{name}


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
%doc BUGS CHANGES CREDITS LICENSE README*
%doc doc/*.txt contrib/init.d/rc.mandriva* contrib/asterisk-ices.xml
%doc contrib/scripts contrib/i18n.testsuite.conf contrib/README.festival
%if %{mgaversion} >= 3
%{_unitdir}/%{name}.service
%attr(0644,root,root) %{_tmpfilesdir}/%{name}.conf
%else
%attr(0755,root,root)					%{_initrddir}/asterisk
%attr(0755,root,root)                                   %{_sbindir}/safe_asterisk
%endif

%attr(0644,root,root) %config(noreplace)		%{_sysconfdir}/logrotate.d/asterisk
%attr(0750,asterisk,asterisk) %dir			%{_sysconfdir}/asterisk
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/*.adsi
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/*.conf
%attr(0644,asterisk,asterisk) %config(noreplace)        %{_sysconfdir}/asterisk/ss7.timers
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/extensions.ael
%attr(0644,asterisk,asterisk) %config(noreplace)	%{_sysconfdir}/asterisk/extensions.lua
%attr(0644,root,root) %config(noreplace)		%{_sysconfdir}/sysconfig/asterisk
# TODO
#attr(0750,root,asterisk) %dir				%{_sysconfdir}/ssl/asterisk
%exclude                                                %{_sysconfdir}/asterisk/ari.conf

# these are packaged as sub packages below
%if %{build_misdn}
%exclude						%{_sysconfdir}/asterisk/misdn.conf
%endif
%if %{build_odbc}
%exclude						%{_sysconfdir}/asterisk/*_odbc.conf
%endif
%if %{build_tds}
%exclude						%{_sysconfdir}/asterisk/*tds*.conf
%exclude                                                %{modulesdir}/cel_tds.so
%exclude                                                %{modulesdir}/cdr_tds.so
%endif
%exclude						%{_sysconfdir}/asterisk/cdr_pgsql.conf
%exclude						%{_sysconfdir}/asterisk/osp.conf
%exclude                                                %{_sysconfdir}/asterisk/ooh323.conf
%exclude						%{_sysconfdir}/asterisk/res_snmp.conf
%exclude                                                %{_sysconfdir}/asterisk/xmpp.conf
%exclude						%{_sysconfdir}/asterisk/*sql*.conf

%attr(0755,root,root)					%{_sbindir}/aelparse
%attr(0755,root,root)					%{_sbindir}/asterisk
%attr(0755,root,root)					%{_sbindir}/astgenkey
%attr(0755,root,root)					%{_sbindir}/astman
%attr(0755,root,root)                                   %{_sbindir}/astversion
%attr(0755,root,root)					%{_sbindir}/autosupport
%attr(0755,root,root)					%{_sbindir}/muted
                                                        %{_sbindir}/rasterisk
%attr(0755,root,root)					%{_sbindir}/smsq
%attr(0755,root,root)					%{_sbindir}/stereorize
%attr(0755,root,root)					%{_sbindir}/streamplayer
%attr(0755,root,root)					%{_sbindir}/astcanary
%attr(0755,root,root)					%{_sbindir}/conf2ael
%attr(0755,root,root)                                   %{_sbindir}/astdb2bdb
%attr(0755,root,root)					%{_sbindir}/astdb2sqlite3

%attr(0755,root,root)	%dir			%{_libdir}/asterisk
%attr(0755,root,root)                                   %{_libdir}/libasteriskssl.so.*
                                                        %{_libdir}/libasteriskssl.so

%attr(0755,root,root) %dir %{modulesdir}
%attr(0755,root,root) %{modulesdir}/app_adsiprog.so
%attr(0755,root,root) %{modulesdir}/app_alarmreceiver.so
%attr(0755,root,root) %{modulesdir}/app_agent_pool.so
%attr(0755,root,root) %{modulesdir}/app_amd.so
%attr(0755,root,root) %{modulesdir}/app_authenticate.so
%attr(0755,root,root) %{modulesdir}/app_cdr.so
%attr(0755,root,root) %{modulesdir}/app_chanisavail.so
%attr(0755,root,root) %{modulesdir}/app_channelredirect.so
%attr(0755,root,root) %{modulesdir}/app_chanspy.so
%attr(0755,root,root) %{modulesdir}/app_controlplayback.so
%attr(0755,root,root) %{modulesdir}/app_confbridge.so
%attr(0755,root,root) %{modulesdir}/app_db.so
%attr(0755,root,root) %{modulesdir}/app_dial.so
%attr(0755,root,root) %{modulesdir}/app_dictate.so
%attr(0755,root,root) %{modulesdir}/app_directed_pickup.so
%attr(0755,root,root) %{modulesdir}/app_disa.so
%attr(0755,root,root) %{modulesdir}/app_dumpchan.so
%attr(0755,root,root) %{modulesdir}/app_echo.so
%attr(0755,root,root) %{modulesdir}/app_exec.so
%attr(0755,root,root) %{modulesdir}/app_externalivr.so
%attr(0755,root,root) %{modulesdir}/app_followme.so
%attr(0755,root,root) %{modulesdir}/app_forkcdr.so
%attr(0755,root,root) %{modulesdir}/app_getcpeid.so
%attr(0755,root,root) %{modulesdir}/app_image.so
# attr(0755,root,root) %{modulesdir}/app_ivrdemo.so
%attr(0755,root,root) %{modulesdir}/app_macro.so
%attr(0755,root,root) %{modulesdir}/app_milliwatt.so
%attr(0755,root,root) %{modulesdir}/app_mixmonitor.so
%attr(0755,root,root) %{modulesdir}/app_morsecode.so
%attr(0755,root,root) %{modulesdir}/app_mp3.so
%attr(0755,root,root) %{modulesdir}/app_nbscat.so
%attr(0755,root,root) %{modulesdir}/app_playback.so
%attr(0755,root,root) %{modulesdir}/app_privacy.so
%attr(0755,root,root) %{modulesdir}/app_queue.so
%attr(0755,root,root) %{modulesdir}/app_readexten.so
%attr(0755,root,root) %{modulesdir}/app_read.so
%attr(0755,root,root) %{modulesdir}/app_record.so
%attr(0755,root,root) %{modulesdir}/app_bridgewait.so
%attr(0755,root,root) %{modulesdir}/app_stasis.so
%attr(0755,root,root) %{modulesdir}/res_timing_timerfd.so
%attr(0755,root,root) %{modulesdir}/app_sayunixtime.so
%attr(0755,root,root) %{modulesdir}/app_senddtmf.so
%attr(0755,root,root) %{modulesdir}/app_sendtext.so
%attr(0755,root,root) %{modulesdir}/app_sms.so
%attr(0755,root,root) %{modulesdir}/app_softhangup.so
%attr(0755,root,root) %{modulesdir}/app_speech_utils.so
%attr(0755,root,root) %{modulesdir}/app_stack.so
%attr(0755,root,root) %{modulesdir}/app_system.so
%attr(0755,root,root) %{modulesdir}/app_talkdetect.so
%attr(0755,root,root) %{modulesdir}/app_test.so
%attr(0755,root,root) %{modulesdir}/app_transfer.so
%attr(0755,root,root) %{modulesdir}/app_url.so
%attr(0755,root,root) %{modulesdir}/app_userevent.so
%attr(0755,root,root) %{modulesdir}/app_waitforring.so
%attr(0755,root,root) %{modulesdir}/app_waitforsilence.so
%attr(0755,root,root) %{modulesdir}/app_waituntil.so
%attr(0755,root,root) %{modulesdir}/app_verbose.so
%attr(0755,root,root) %{modulesdir}/app_while.so
%attr(0755,root,root) %{modulesdir}/app_zapateller.so
%attr(0755,root,root) %{modulesdir}/bridge*.so
%attr(0755,root,root) %{modulesdir}/cdr_csv.so
%attr(0755,root,root) %{modulesdir}/cdr_custom.so
%attr(0755,root,root) %{modulesdir}/cdr_manager.so
%attr(0755,root,root) %{modulesdir}/chan_bridge_media.so
%attr(0755,root,root) %{modulesdir}/chan_iax2.so
%attr(0755,root,root) %{modulesdir}/chan_motif.so
%attr(0755,root,root) %{modulesdir}/chan_phone.so
%attr(0755,root,root) %{modulesdir}/chan_sip.so
%attr(0755,root,root) %{modulesdir}/chan_mgcp.so

%attr(0755,root,root) %{modulesdir}/codec_adpcm.so
%attr(0755,root,root) %{modulesdir}/codec_alaw.so
%attr(0755,root,root) %{modulesdir}/codec_a_mu.so
%attr(0755,root,root) %{modulesdir}/codec_g722.so
%attr(0755,root,root) %{modulesdir}/codec_g726.so
%attr(0755,root,root) %{modulesdir}/codec_gsm.so
%attr(0755,root,root) %{modulesdir}/codec_ilbc.so
%attr(0755,root,root) %{modulesdir}/codec_lpc10.so
%attr(0755,root,root) %{modulesdir}/codec_resample.so
%attr(0755,root,root) %{modulesdir}/codec_ulaw.so
%attr(0755,root,root) %{modulesdir}/format_g719.so
%attr(0755,root,root) %{modulesdir}/format_g723.so
%attr(0755,root,root) %{modulesdir}/format_g726.so
%attr(0755,root,root) %{modulesdir}/format_g729.so
%attr(0755,root,root) %{modulesdir}/format_gsm.so
%attr(0755,root,root) %{modulesdir}/format_h263.so
%attr(0755,root,root) %{modulesdir}/format_h264.so
%attr(0755,root,root) %{modulesdir}/format_ilbc.so
%attr(0755,root,root) %{modulesdir}/format_jpeg.so
%attr(0755,root,root) %{modulesdir}/format_ogg_vorbis.so
%attr(0755,root,root) %{modulesdir}/format_pcm.so
%attr(0755,root,root) %{modulesdir}/format_sln.so
%attr(0755,root,root) %{modulesdir}/format_siren14.so
%attr(0755,root,root) %{modulesdir}/format_siren7.so
%attr(0755,root,root) %{modulesdir}/func_aes.so
#%attr(0755,root,root) %{modulesdir}/func_connectedline.so
%attr(0755,root,root) %{modulesdir}/format_wav_gsm.so
%attr(0755,root,root) %{modulesdir}/format_wav.so
%attr(0755,root,root) %{modulesdir}/format_vox.so
%attr(0755,root,root) %{modulesdir}/func_audiohookinherit.so
%attr(0755,root,root) %{modulesdir}/func_base64.so
%attr(0755,root,root) %{modulesdir}/func_blacklist.so
%attr(0755,root,root) %{modulesdir}/func_callerid.so
%attr(0755,root,root) %{modulesdir}/func_cdr.so
%attr(0755,root,root) %{modulesdir}/func_channel.so
%attr(0755,root,root) %{modulesdir}/func_callcompletion.so
%attr(0755,root,root) %{modulesdir}/func_config.so
%attr(0755,root,root) %{modulesdir}/func_cut.so
%attr(0755,root,root) %{modulesdir}/func_db.so
%attr(0755,root,root) %{modulesdir}/func_devstate.so
%attr(0755,root,root) %{modulesdir}/func_dialgroup.so
%attr(0755,root,root) %{modulesdir}/func_dialplan.so
%attr(0755,root,root) %{modulesdir}/func_enum.so
%attr(0755,root,root) %{modulesdir}/func_env.so
%attr(0755,root,root) %{modulesdir}/func_extstate.so
%attr(0755,root,root) %{modulesdir}/func_frame_trace.so
%attr(0755,root,root) %{modulesdir}/func_global.so
%attr(0755,root,root) %{modulesdir}/func_groupcount.so
%attr(0755,root,root) %{modulesdir}/func_hangupcause.so
%attr(0755,root,root) %{modulesdir}/func_holdintercept.so
%attr(0755,root,root) %{modulesdir}/func_iconv.so
%attr(0755,root,root) %{modulesdir}/func_jitterbuffer.so
%attr(0755,root,root) %{modulesdir}/func_lock.so
%attr(0755,root,root) %{modulesdir}/func_logic.so
%attr(0755,root,root) %{modulesdir}/func_math.so
%attr(0755,root,root) %{modulesdir}/func_md5.so
%attr(0755,root,root) %{modulesdir}/func_module.so
%attr(0755,root,root) %{modulesdir}/func_rand.so
%attr(0755,root,root) %{modulesdir}/func_realtime.so
%attr(0755,root,root) %{modulesdir}/func_sha1.so
%attr(0755,root,root) %{modulesdir}/func_shell.so
%attr(0755,root,root) %{modulesdir}/func_strings.so
%attr(0755,root,root) %{modulesdir}/func_srv.so
%attr(0755,root,root) %{modulesdir}/func_periodic_hook.so
%attr(0755,root,root) %{modulesdir}/func_presencestate.so
%attr(0755,root,root) %{modulesdir}/func_pitchshift.so
%attr(0755,root,root) %{modulesdir}/func_sorcery.so
%attr(0755,root,root) %{modulesdir}/func_sysinfo.so
%attr(0755,root,root) %{modulesdir}/func_talkdetect.so
%attr(0755,root,root) %{modulesdir}/func_timeout.so
%attr(0755,root,root) %{modulesdir}/func_uri.so
%attr(0755,root,root) %{modulesdir}/func_version.so
%attr(0755,root,root) %{modulesdir}/func_volume.so
%attr(0755,root,root) %{modulesdir}/pbx_ael.so
%attr(0755,root,root) %{modulesdir}/pbx_config.so
%attr(0755,root,root) %{modulesdir}/pbx_dundi.so
# attr(0755,root,root) %{modulesdir}/pbx_gtkconsole.so
%attr(0755,root,root) %{modulesdir}/pbx_loopback.so
%attr(0755,root,root) %{modulesdir}/pbx_realtime.so
%attr(0755,root,root) %{modulesdir}/pbx_spool.so
%attr(0755,root,root) %{modulesdir}/res_adsi.so
%attr(0755,root,root) %{modulesdir}/res_ael_share.so
%attr(0755,root,root) %{modulesdir}/res_agi.so
%attr(0755,root,root) %{modulesdir}/res_clioriginate.so
%attr(0755,root,root) %{modulesdir}/res_convert.so
%attr(0755,root,root) %{modulesdir}/res_crypto.so
%attr(0755,root,root) %{modulesdir}/res_http_post.so
# attr(0755,root,root) %{modulesdir}/res_indications.so
%attr(0755,root,root) %{modulesdir}/res_limit.so
%attr(0755,root,root) %{modulesdir}/res_monitor.so
%attr(0755,root,root) %{modulesdir}/res_musiconhold.so
%attr(0755,root,root) %{modulesdir}/res_phoneprov.so
%attr(0755,root,root) %{modulesdir}/res_realtime.so
%attr(0755,root,root) %{modulesdir}/res_smdi.so
%attr(0755,root,root) %{modulesdir}/res_speech.so
%attr(0755,root,root) %{modulesdir}/res_stun_monitor.so
%attr(0755,root,root) %{modulesdir}/res_timing_pthread.so
# %attr(0755,root,root) %{modulesdir}/test_dlinklists.so
# %attr(0755,root,root) %{modulesdir}/test_heap.so
# %attr(0755,root,root) %{modulesdir}/func_redirecting.so
%attr(0755,root,root) %{modulesdir}/func_sprintf.so
%attr(0755,root,root) %{modulesdir}/res_clialiases.so
%attr(0755,root,root) %{modulesdir}/res_rtp_asterisk.so
%attr(0755,root,root) %{modulesdir}/app_originate.so
%attr(0755,root,root) %{modulesdir}/app_playtones.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_celt.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_silk.so

# Do these really belong here? :
%attr(0755,root,root) %{modulesdir}/app_celgenuserevent.so
%attr(0755,root,root) %{modulesdir}/cdr_syslog.so
%attr(0755,root,root) %{modulesdir}/cel_custom.so
%attr(0755,root,root) %{modulesdir}/cel_manager.so
%attr(0755,root,root) %{modulesdir}/chan_mobile.so
%attr(0755,root,root) %{modulesdir}/chan_rtp.so
# %attr(0755,root,root) %{modulesdir}/format_mp3.so
%attr(0755,root,root) %{modulesdir}/res_calendar.so
%attr(0755,root,root) %{modulesdir}/res_calendar_caldav.so
%attr(0755,root,root) %{modulesdir}/res_calendar_exchange.so
%attr(0755,root,root) %{modulesdir}/res_calendar_ews.so
%attr(0755,root,root) %{modulesdir}/res_calendar_icalendar.so
%attr(0755,root,root) %{modulesdir}/res_mutestream.so
%attr(0755,root,root) %{modulesdir}/res_rtp_multicast.so
%attr(0755,root,root) %{modulesdir}/res_security_log.so
%attr(0755,root,root) %{modulesdir}/res_config_sqlite3.so

%attr(0755,root,root) %{modulesdir}/res_format_attr_h263.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_h264.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_opus.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_vp8.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_g729.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_siren7.so
%attr(0755,root,root) %{modulesdir}/res_format_attr_siren14.so
%attr(0755,root,root) %{modulesdir}/res_hep.so
%attr(0755,root,root) %{modulesdir}/res_hep_rtcp.so
%attr(0755,root,root) %{modulesdir}/res_http_websocket.so
%attr(0755,root,root) %{modulesdir}/res_manager_devicestate.so
%attr(0755,root,root) %{modulesdir}/res_manager_presencestate.so
%attr(0755,root,root) %{modulesdir}/res_parking.so
%attr(0755,root,root) %{modulesdir}/res_sorcery_astdb.so
%attr(0755,root,root) %{modulesdir}/res_sorcery_config.so
%attr(0755,root,root) %{modulesdir}/res_sorcery_memory.so
%attr(0755,root,root) %{modulesdir}/res_sorcery_realtime.so
%attr(0755,root,root) %{modulesdir}/res_sorcery_memory_cache.so
%attr(0755,root,root) %{modulesdir}/res_stasis.so
%attr(0755,root,root) %{modulesdir}/res_stasis_answer.so
%attr(0755,root,root) %{modulesdir}/res_stasis_device_state.so
%attr(0755,root,root) %{modulesdir}/res_stasis_playback.so
%attr(0755,root,root) %{modulesdir}/res_stasis_recording.so
%attr(0755,root,root) %{modulesdir}/res_stasis_snoop.so
%attr(0755,root,root) %{modulesdir}/res_statsd.so


%attr(0755,asterisk,asterisk)	%dir			%{astvardir}
%ghost							%{astvardir}/astdb
%attr(0755,root,root)		%dir			%{astvardir}/agi-bin 
%attr(0755,root,root)		%dir			%{astvardir}/images
%attr(0644,root,root)					%{astvardir}/images/*.jpg
%attr(0755,root,root)		%dir			%{astvardir}/static-http
%attr(0644,root,root)					%{astvardir}/static-http/*
%attr(0644,root,root)					%{astvardir}/phoneprov/*.cfg
%attr(0644,root,root)					%{astvardir}/phoneprov/*.xml
%attr(0755,asterisk,asterisk)	%dir			%{astvardir}/documentation
%attr(-,asterisk,asterisk)				%{astvardir}/documentation/*

%if 0
# *-* Revise, where are they now?
%attr(0755,root,root)		%dir			%{astvardir}/firmware
%attr(0755,root,root)		%dir			%{astvardir}/firmware/iax
%attr(0755,root,root)					%{astvardir}/firmware/iax/*.bin
%attr(0755,root,root)		%dir			%{astvardir}/keys
%attr(0644,root,root)					%{astvardir}/keys/*.pub
%endif

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


							%{_mandir}/man8/asterisk.8*
							%{_mandir}/man8/astgenkey.8*
							%{_mandir}/man8/autosupport.8*
							%{_mandir}/man8/safe_asterisk.8*
                                                        %{_mandir}/man8/astdb2bdb.8*
                                                        %{_mandir}/man8/astdb2sqlite3.8*
%exclude /var/www/
%exclude %{modulesdir}/app_flash.so
%exclude %{modulesdir}/app_page.so
%exclude %{modulesdir}/app_dahdiras.so
# exclude %{modulesdir}/app_dahdiscan.so
%exclude %{modulesdir}/chan_dahdi.so
%exclude %{modulesdir}/codec_dahdi.so
%exclude %{modulesdir}/res_timing_dahdi.so
%exclude %{modulesdir}/chan_ooh323.so


%files devel -f %{name}-devel.filelist
%defattr(-,root,root,-)
# *-* %doc doc/CODING-GUIDELINES doc/datastores.txt doc/modules.txt doc/valgrind.txt
%dir %{_includedir}/asterisk
%{_includedir}/asterisk.h
%{_includedir}/asterisk/*.h
%{_includedir}/asterisk/doxygen/*.h
# %{_libdir}/pkgconfig/asterisk.pc

%files firmware
%defattr(-,root,root,-)
# *-*
# %attr(0750,asterisk,asterisk) /var/lib/asterisk/firmware


%files plugins-alsa
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/alsa.conf
%attr(0755,root,root) %{modulesdir}/chan_alsa.so

%files plugins-curl
%defattr(-,root,root,-)
# %doc contrib/scripts/dbsep.cgi
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/dbsep.conf
%attr(0755,root,root) %{modulesdir}/func_curl.so
%attr(0755,root,root) %{modulesdir}/res_config_curl.so
%attr(0755,root,root) %{modulesdir}/res_curl.so

%if %{build_dahdi}
%files plugins-dahdi
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/meetme.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/chan_dahdi.conf
                        %{_datadir}/dahdi/span_config.d/40-asterisk
%attr(0755,root,root) %{modulesdir}/app_flash.so
%attr(0755,root,root) %{modulesdir}/app_page.so
%attr(0755,root,root) %{modulesdir}/app_dahdiras.so
#attr(0755,root,root) %{modulesdir}/app_dahdiscan.so
%attr(0755,root,root) %{modulesdir}/chan_dahdi.so
%attr(0755,root,root) %{modulesdir}/codec_dahdi.so
%attr(0755,root,root) %{modulesdir}/res_timing_dahdi.so
%endif

%files plugins-fax
%defattr(-,root,root,-)
# attr(0755,root,root) %{modulesdir}/app_fax.so
%attr(0755,root,root) %{modulesdir}/res_fax.so
%attr(0755,root,root) %{modulesdir}/res_fax_spandsp.so

%files plugins-festival
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/festival.conf
%attr(0755,root,root) %{modulesdir}/app_festival.so

%files plugins-ices
%defattr(-,root,root,-)
%attr(0755,root,root) %{modulesdir}/app_ices.so

%files plugins-jabber
%defattr(-,root,root,-)
# %doc doc/jabber.txt doc/jingle.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/xmpp.conf
%attr(0755,root,root) %{modulesdir}/res_xmpp.so

%files plugins-jack
%defattr(-,root,root,-)
%attr(0755,root,root) %{modulesdir}/app_jack.so

%files plugins-lua
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/extensions.lua
%attr(0755,root,root) %{modulesdir}/pbx_lua.so

%files plugins-ldap
%defattr(-,root,root,-)
# %doc doc/ldap.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_ldap.conf
%attr(0755,root,root) %{modulesdir}/res_config_ldap.so

%files plugins-minivm
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/extensions_minivm.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/minivm.conf
%attr(0755,root,root) %{modulesdir}/app_minivm.so

%if %{build_misdn}
%files plugins-misdn
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/misdn.conf
%attr(0755,root,root) %{modulesdir}/chan_misdn.so
%endif

%files plugins-mysql
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_mysql.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_config_mysql.conf
%attr(0755,root,root) %{modulesdir}/res_config_mysql.so

%if %{build_odbc}
%files plugins-odbc
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_adaptive_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/func_odbc.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_odbc.conf
%attr(0755,root,root) %{modulesdir}/cdr_adaptive_odbc.so
%attr(0755,root,root) %{modulesdir}/cdr_odbc.so
%attr(0755,root,root) %{modulesdir}/func_odbc.so
%attr(0755,root,root) %{modulesdir}/res_config_odbc.so
%attr(0755,root,root) %{modulesdir}/res_odbc_transaction.so
%attr(0755,root,root) %{modulesdir}/res_odbc.so
%attr(0755,root,root) %{modulesdir}/cel_odbc.so
%endif

%files plugins-oss
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/oss.conf
%attr(0755,root,root) %{modulesdir}/chan_oss.so

%if %{build_osp}
%files plugins-osp
%defattr(-,root,root)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/osp.conf
%attr(0755,root,root) %{modulesdir}/app_osplookup.so
%endif

%files plugins-portaudio
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/console.conf
%attr(0755,root,root) %{modulesdir}/chan_console.so

%files plugins-pgsql
%defattr(-,root,root,-)
# doc contrib/realtime/postgresql/realtime.sql
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_pgsql.conf
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_pgsql.conf
%attr(0755,root,root) %{modulesdir}/cdr_pgsql.so
%attr(0755,root,root) %{modulesdir}/res_config_pgsql.so
%attr(0755,root,root) %{modulesdir}/cel_pgsql.so

%files plugins-radius
%defattr(-,root,root,-)
%attr(0755,root,root) %{modulesdir}/cdr_radius.so
%attr(0755,root,root) %{modulesdir}/cel_radius.so


%files plugins-skinny
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/skinny.conf
%attr(0755,root,root) %{modulesdir}/chan_skinny.so

%files plugins-snmp
%defattr(-,root,root,-)
# %doc doc/asterisk-mib.txt
# %doc doc/digium-mib.txt
# %doc doc/snmp.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/res_snmp.conf
%attr(0755,root,root) %{modulesdir}/res_snmp.so

%files plugins-sqlite
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_sqlite3_custom.conf
# attr(0755,root,root) %{modulesdir}/cdr_sqlite.so
%attr(0755,root,root) %{modulesdir}/cdr_sqlite3_custom.so
%attr(0755,root,root) %{modulesdir}/res_config_sqlite.so
%attr(0755,root,root) %{modulesdir}/cel_sqlite3_custom.so

%files plugins-speex
%defattr(-,root,root,-)
%attr(0755,root,root) %{modulesdir}/codec_speex.so
%attr(0755,root,root) %{modulesdir}/func_speex.so

%if %{build_tds}
%files plugins-tds
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/cdr_tds.conf
%attr(0755,root,root) %{modulesdir}/cdr_tds.so
%attr(0755,root,root) %{modulesdir}/cel_tds.so
%endif

%files plugins-unistim
%defattr(-,root,root,-)
# %doc doc/unistim.txt
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/unistim.conf
%attr(0755,root,root) %{modulesdir}/chan_unistim.so

%if 0
%files plugins-usbradio
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/usbradio.conf
%attr(0755,root,root) %{modulesdir}/chan_usbradio.so
%endif

%files plugins-voicemail
%defattr(-,root,root,-)
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/voicemail.conf
%attr(0755,root,root) %{modulesdir}/func_vmcount.so
%attr(0750,asterisk,asterisk)   %dir                    /var/spool/asterisk/voicemail
%attr(0750,asterisk,asterisk)   %dir                    /var/spool/asterisk/voicemail/default
%attr(0750,asterisk,asterisk)   %dir                    /var/spool/asterisk/voicemail/default/1234
%attr(0750,asterisk,asterisk)   %dir                    /var/spool/asterisk/voicemail/default/1234/INBOX
%attr(0644,asterisk,asterisk)                           /var/spool/asterisk/voicemail/default/1234/en/*
#attr(0750,asterisk,asterisk)   %dir                    /var/spool/asterisk/voicemail/voicemail


%files plugins-ooh323
%attr(0640,asterisk,asterisk) %config(noreplace) %{_sysconfdir}/asterisk/ooh323.conf
%attr(0755,root,root) %{modulesdir}/chan_ooh323.so


%if %{build_imap}
#FIXME: find a better way to build dir_imap etc.
%files plugins-voicemail-imap
%defattr(-,root,root,-)
%attr(0755,root,root) %{modulesdir}/app_directory_imap.so
%attr(0755,root,root) %{modulesdir}/app_voicemail_imap.so
%endif

%if 0
# FIXME
%if %{build_odbc}
%files plugins-voicemail-odbc
%defattr(-,root,root,-)
%doc doc/voicemail_odbc_postgresql.txt
%attr(0755,root,root) %{modulesdir}/app_directory_odbc.so
%attr(0755,root,root) %{modulesdir}/app_voicemail_odbc.so
%endif
%endif

#FIXME!
%files plugins-voicemail-plain
%defattr(-,root,root,-)
# attr(0755,root,root) %{modulesdir}/app_directory_plain.so
# attr(0755,root,root) %{modulesdir}/app_voicemail_plain.so
%attr(0755,root,root) %{modulesdir}/app_directory.so
%attr(0755,root,root) %{modulesdir}/app_voicemail.so

%files ari
%attr(0644,asterisk,asterisk) %config(noreplace)        %{_sysconfdir}/asterisk/ari.conf
%attr(0755,root,root) %{modulesdir}/res_ari.so
%attr(0755,root,root) %{modulesdir}/res_ari_applications.so
%attr(0755,root,root) %{modulesdir}/res_ari_asterisk.so
%attr(0755,root,root) %{modulesdir}/res_ari_bridges.so
%attr(0755,root,root) %{modulesdir}/res_ari_channels.so
%attr(0755,root,root) %{modulesdir}/res_ari_device_states.so
%attr(0755,root,root) %{modulesdir}/res_ari_endpoints.so
%attr(0755,root,root) %{modulesdir}/res_ari_events.so
# %attr(0755,root,root) %{modulesdir}/res_ari_mailboxes.so
%attr(0755,root,root) %{modulesdir}/res_ari_model.so
%attr(0755,root,root) %{modulesdir}/res_ari_playbacks.so
%attr(0755,root,root) %{modulesdir}/res_ari_recordings.so
%attr(0755,root,root) %{modulesdir}/res_ari_sounds.so
%attr(0755,asterisk,asterisk) %dir %{astvardir}/rest-api/
%attr(0640,asterisk,asterisk) %config(noreplace) %{astvardir}/rest-api/*.json



%files docs
%defattr(-,root,root)
# %doc	docs-html/*

%files sounds-core-en-alaw
%defattr(-,root, root)
                %{soundsdir}/en/CHANGES-asterisk-core-en-%{sounds_core_version}
                %{soundsdir}/en/CREDITS-asterisk-core-en-%{sounds_core_version}
                %{soundsdir}/en/LICENSE-asterisk-core-en-%{sounds_core_version}
#doc en/CREDITS-asterisk-core-en-%{version}
%attr(644,root,root)    %{soundsdir}/en/*.alaw
%attr(644,root,root)    %{soundsdir}/en/dictate/*.alaw
%attr(644,root,root)    %{soundsdir}/en/digits/*.alaw
%attr(644,root,root)    %{soundsdir}/en/followme/*.alaw
%attr(644,root,root)    %{soundsdir}/en/letters/*.alaw
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.alaw
%attr(644,root,root)    %{soundsdir}/en/silence/*.alaw

%files sounds-core-en-ulaw
%defattr(-,root, root)
#doc en/CHANGES-asterisk-core-en-%{version}
#doc en/CREDITS-asterisk-core-en-%{version}
%attr(644,root,root)    %{soundsdir}/en/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/dictate/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/digits/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/followme/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/letters/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.ulaw
%attr(644,root,root)    %{soundsdir}/en/silence/*.ulaw

%if 0
%files sounds-core-en-g722
%defattr(-,root, root)
%attr(644,root,root)    %{soundsdir}/en/*.g722
%attr(644,root,root)    %{soundsdir}/en/dictate/*.g722
%attr(644,root,root)    %{soundsdir}/en/digits/*.g722
%attr(644,root,root)    %{soundsdir}/en/followme/*.g722
%attr(644,root,root)    %{soundsdir}/en/letters/*.g722
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.g722
%attr(644,root,root)    %{soundsdir}/en/silence/*.g722
%endif

%files sounds-core-en-g729
%defattr(-,root, root)
%attr(644,root,root)    %{soundsdir}/en/*.g729
%attr(644,root,root)    %{soundsdir}/en/dictate/*.g729
%attr(644,root,root)    %{soundsdir}/en/digits/*.g729
%attr(644,root,root)    %{soundsdir}/en/followme/*.g729
%attr(644,root,root)    %{soundsdir}/en/letters/*.g729
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.g729
%attr(644,root,root)    %{soundsdir}/en/silence/*.g729

%files sounds-core-en-wav
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/en/*.wav
%attr(644,root,root)    %{soundsdir}/en/dictate/*.wav
%attr(644,root,root)    %{soundsdir}/en/digits/*.wav
%attr(644,root,root)    %{soundsdir}/en/followme/*.wav
%attr(644,root,root)    %{soundsdir}/en/letters/*.wav
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.wav
%attr(644,root,root)    %{soundsdir}/en/silence/*.wav

%files sounds-core-en-sln16
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/en/*.sln16
%attr(644,root,root)    %{soundsdir}/en/dictate/*.sln16
%attr(644,root,root)    %{soundsdir}/en/digits/*.sln16
%attr(644,root,root)    %{soundsdir}/en/followme/*.sln16
%attr(644,root,root)    %{soundsdir}/en/letters/*.sln16
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.sln16
%attr(644,root,root)    %{soundsdir}/en/silence/*.sln16

%files sounds-core-en-gsm
%defattr(-,root, root)
#doc en/CHANGES-asterisk-core-en-%{version}
#doc en/CREDITS-asterisk-core-en-%{version}
%attr(644,root,root)    %{soundsdir}/en/*.gsm
%attr(644,root,root)    %{soundsdir}/en/dictate/*.gsm
%attr(644,root,root)    %{soundsdir}/en/digits/*.gsm
%attr(644,root,root)    %{soundsdir}/en/followme/*.gsm
%attr(644,root,root)    %{soundsdir}/en/letters/*.gsm
%attr(644,root,root)    %{soundsdir}/en/phonetic/*.gsm
%attr(644,root,root)    %{soundsdir}/en/silence/*.gsm

%if 0
%files sounds-core-fr-alaw
%defattr(-,root, root)
%doc fr/CHANGES-asterisk-core-fr-%{version}
%doc fr/CREDITS-asterisk-core-fr-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/digits/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/followme/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/letters/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.alaw
%attr(644,root,root)    %{soundsdir}/fr/silence/*.alaw

%files sounds-core-fr-ulaw
%defattr(-,root, root)
%doc fr/CHANGES-asterisk-core-fr-%{version}
%doc fr/CREDITS-asterisk-core-fr-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/digits/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/followme/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/letters/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.ulaw
%attr(644,root,root)    %{soundsdir}/fr/silence/*.ulaw

%files sounds-core-fr-gsm
%defattr(-,root, root)
%doc fr/CHANGES-asterisk-core-fr-%{version}
%doc fr/CREDITS-asterisk-core-fr-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/digits/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/followme/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/letters/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.gsm
%attr(644,root,root)    %{soundsdir}/fr/silence/*.gsm

%files sounds-core-fr-g722
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.g722
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.g722
%attr(644,root,root)    %{soundsdir}/fr/digits/*.g722
%attr(644,root,root)    %{soundsdir}/fr/followme/*.g722
%attr(644,root,root)    %{soundsdir}/fr/letters/*.g722
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.g722
%attr(644,root,root)    %{soundsdir}/fr/silence/*.g722

%files sounds-core-fr-g729
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.g729
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.g729
%attr(644,root,root)    %{soundsdir}/fr/digits/*.g729
%attr(644,root,root)    %{soundsdir}/fr/followme/*.g729
%attr(644,root,root)    %{soundsdir}/fr/letters/*.g729
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.g729
%attr(644,root,root)    %{soundsdir}/fr/silence/*.g729

%files sounds-core-fr-wav
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/fr/*.wav
%attr(644,root,root)    %{soundsdir}/fr/dictate/*.wav
%attr(644,root,root)    %{soundsdir}/fr/digits/*.wav
%attr(644,root,root)    %{soundsdir}/fr/followme/*.wav
%attr(644,root,root)    %{soundsdir}/fr/letters/*.wav
%attr(644,root,root)    %{soundsdir}/fr/phonetic/*.wav
%attr(644,root,root)    %{soundsdir}/fr/silence/*.wav

%files sounds-core-es-alaw
%defattr(-,root, root)
%doc es/CHANGES-asterisk-core-es-%{version}
%doc es/CREDITS-asterisk-core-es-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.alaw
%attr(644,root,root)    %{soundsdir}/es/dictate/*.alaw
%attr(644,root,root)    %{soundsdir}/es/digits/*.alaw
%attr(644,root,root)    %{soundsdir}/es/followme/*.alaw
%attr(644,root,root)    %{soundsdir}/es/letters/*.alaw
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.alaw
%attr(644,root,root)    %{soundsdir}/es/silence/*.alaw

%files sounds-core-es-ulaw
%defattr(-,root, root)
%doc es/CHANGES-asterisk-core-es-%{version}
%doc es/CREDITS-asterisk-core-es-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/dictate/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/digits/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/followme/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/letters/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.ulaw
%attr(644,root,root)    %{soundsdir}/es/silence/*.ulaw

%files sounds-core-es-gsm
%defattr(-,root, root)
%doc es/CHANGES-asterisk-core-es-%{version}
%doc es/CREDITS-asterisk-core-es-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.gsm
%attr(644,root,root)    %{soundsdir}/es/dictate/*.gsm
%attr(644,root,root)    %{soundsdir}/es/digits/*.gsm
%attr(644,root,root)    %{soundsdir}/es/followme/*.gsm
%attr(644,root,root)    %{soundsdir}/es/letters/*.gsm
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.gsm
%attr(644,root,root)    %{soundsdir}/es/silence/*.gsm

%files sounds-core-es-g722
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.g722
%attr(644,root,root)    %{soundsdir}/es/dictate/*.g722
%attr(644,root,root)    %{soundsdir}/es/digits/*.g722
%attr(644,root,root)    %{soundsdir}/es/followme/*.g722
%attr(644,root,root)    %{soundsdir}/es/letters/*.g722
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.g722
%attr(644,root,root)    %{soundsdir}/es/silence/*.g722

%files sounds-core-es-g729
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.g729
%attr(644,root,root)    %{soundsdir}/es/dictate/*.g729
%attr(644,root,root)    %{soundsdir}/es/digits/*.g729
%attr(644,root,root)    %{soundsdir}/es/followme/*.g729
%attr(644,root,root)    %{soundsdir}/es/letters/*.g729
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.g729
%attr(644,root,root)    %{soundsdir}/es/silence/*.g729

%files sounds-core-es-wav
%defattr(-,root, root)
#%doc CREDITS-asterisk-core-*-%{version}
%attr(644,root,root)    %{soundsdir}/es/*.wav
%attr(644,root,root)    %{soundsdir}/es/dictate/*.wav
%attr(644,root,root)    %{soundsdir}/es/digits/*.wav
%attr(644,root,root)    %{soundsdir}/es/followme/*.wav
%attr(644,root,root)    %{soundsdir}/es/letters/*.wav
%attr(644,root,root)    %{soundsdir}/es/phonetic/*.wav
%attr(644,root,root)    %{soundsdir}/es/silence/*.wav

%endif

%files moh-opsound-alaw
%defattr(-,root, root)
                        %{mohdir}/LICENSE-asterisk-moh-opsound-alaw
                        %{mohdir}/CHANGES-asterisk-moh-opsound-alaw
                        %{mohdir}/CREDITS-asterisk-moh-opsound-alaw
%attr(644,root,root)    %{mohdir}/*.alaw

%files moh-opsound-ulaw
%defattr(-,root, root)
                        %{mohdir}/LICENSE-asterisk-moh-opsound-ulaw
                        %{mohdir}/CHANGES-asterisk-moh-opsound-ulaw
                        %{mohdir}/CREDITS-asterisk-moh-opsound-ulaw
%attr(644,root,root)    %{mohdir}/*.ulaw

%files moh-opsound-g729
%defattr(-,root, root)
                        %{mohdir}/LICENSE-asterisk-moh-opsound-g729
                        %{mohdir}/CHANGES-asterisk-moh-opsound-g729
                        %{mohdir}/CREDITS-asterisk-moh-opsound-g729
#doc moh/LICENSE-asterisk-moh-opsound-g729
%attr(644,root,root)    %{mohdir}/*.g729

%files moh-opsound-sln16
%defattr(-,root, root)
                        %{mohdir}/LICENSE-asterisk-moh-opsound-sln16
                        %{mohdir}/CHANGES-asterisk-moh-opsound-sln16
                        %{mohdir}/CREDITS-asterisk-moh-opsound-sln16
%attr(644,root,root)    %{mohdir}/*.sln16

%files moh-opsound-wav
%defattr(-,root, root)
                        %{mohdir}/LICENSE-asterisk-moh-opsound-wav
                        %{mohdir}/CHANGES-asterisk-moh-opsound-wav
                        %{mohdir}/CREDITS-asterisk-moh-opsound-wav
%attr(644,root,root)    %{mohdir}/*.wav


%if 0
%files moh-opsound-g722
%defattr(-,root, root)
#doc moh/LICENSE-asterisk-moh-opsound-g722
%attr(644,root,root)    %{mohdir}/*.g722

%files moh-opsound-gsm
%defattr(-,root, root)
#doc moh/LICENSE-asterisk-moh-opsound-gsm
%attr(644,root,root)    %{mohdir}/*.gsm

%endif


%files tests
%if 0
# They would depend on the TEST_FRAMEWORK option
%attr(0755,root,root)  %{modulesdir}/test_dlinklists.so
%attr(0755,root,root)  %{modulesdir}/test_sched.so
%attr(0755,root,root)  	%{modulesdir}/test_logger.so
%attr(0755,root,root)  %{modulesdir}/test_substitution.so
%attr(0755,root,root)	%{modulesdir}/test_security_events.so
%attr(0755,root,root)	%{modulesdir}/test_amihooks.so
%endif
%attr(0755,root,root)	%{_sbindir}/check_expr
%attr(0755,root,root)	%{_sbindir}/check_expr2

%changelog -f %{_sourcedir}/%{name}-changelog.gitrpm.txt


