Name: tvheadend
Summary: TV streaming server
Version: 2.12.cae47cf
Release: 1%{dist}
License: GPL
Group: Applications/Multimedia
URL: http://www.lonelycoder.com/tvheadend
Packager: Jouk Hettema
Source: tvheadend-%{version}.tar.bz2
Prefix: /usr
BuildRequires: avahi-devel, openssl, glibc, zlib

%description
Tvheadend is a TV streaming server for Linux supporting DVB-S, DVB-S2, 
DVB-C, DVB-T, ATSC, IPTV, and Analog video (V4L) as input sources.

%prep
%setup -q

%build
%configure --release --prefix=%{prefix}/bin
%{__make}

%install
%{__rm} -rf %{buildroot}

mkdir -p $RPM_BUILD_ROOT/%{prefix}

%{__install} -d -m0755 %{buildroot}%{prefix}/bin
%{__install} -d -m0755 %{buildroot}/etc/tvheadend
%{__install} -d -m0755 %{buildroot}/etc/sysconfig
%{__install} -d -m0755 %{buildroot}/etc/rc.d/init.d
%{__install} -d -m0755 %{buildroot}%{prefix}/shared/man/man1

%{__install} -m0755 build.Linux/tvheadend %{buildroot}%{prefix}/bin/
%{__install} -m0755 man/tvheadend.1 %{buildroot}%{prefix}/shared/man/man1
%{__install} -m0755 contrib/redhat/tvheadend %{buildroot}/etc/rc.d/init.d

cat >> %{buildroot}/etc/sysconfig/tvheadend << EOF
OPTIONS="-f -u tvheadend -g tvheadend -c /etc/tvheadend -s -C"
EOF

%pre
groupadd -f -r tvheadend >/dev/null 2>&! || :
useradd -s /sbin/nologin -c "Tvheadend" -M -r tvheadend -g tvheadend -G video >/dev/null 2>&! || :

%preun
if [ $1 = 0 ]; then
	/sbin/service tvheadend stop >/dev/null 2>&1
	/sbin/chkconfig --del tvheadend
fi

%post
/sbin/chkconfig --add tvheadend

if [ "$1" -ge "1" ]; then
	/sbin/service tvheadend condrestart >/dev/null 2>&1 || :
fi

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%doc debian/changelog LICENSE README
%dir %attr(-,tvheadend,root) %{prefix}
%{prefix}/bin/tvheadend
%{prefix}/shared/man/man1/tvheadend.1*
%{_sysconfdir}/rc.d/init.d/tvheadend
/etc/tvheadend
%config /etc/sysconfig/tvheadend

%changelog
* Fri Feb 18 2011 Jouk Hettema <joukio@gmail.com> - 2.12.cae47cf
- initial build for fedora 14
