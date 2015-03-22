%global commit 501a2bea8f0b5e8adc9495581c3d4284a4f27ab6
%global shortcommit %(c=%{commit}; echo ${c:0:7})

Summary:        Tvheadend - a TV streaming server and DVR
Name:           tvheadend
Version:        3.9.20150319
Release:        5%{?dist}

License:        GPLv3
Group:          Applications/Multimedia
URL:            https://tvheadend.org/projects/tvheadend
# The source was pulled from upstreams git scm. Use the following
# commands to generate the tarball
# git clone https://github.com/tvheadend/tvheadend.git 
# cd tvheadend/
# git archive 501a2bea8f0b5e8adc9495581c3d4284a4f27ab6 --format=tar --prefix=tvheadend/ | gzip > ../tvheadend-3.9.20150319.tar.gz

Source0:	tvheadend-3.9.20150319.tar.gz
#Patch999:      test.patch

BuildRequires:  systemd-units >= 1
BuildRequires:  dbus-devel
BuildRequires:  avahi-libs
BuildRequires:  openssl-devel
BuildRequires:  git 
BuildRequires:  wget
BuildRequires:  ffmpeg-devel
BuildRequires:  libvpx-devel
BuildRequires:  python

Requires:       systemd-units >= 1

%description
Tvheadend is a TV streaming server with DVR for Linux supporting
DVB, ATSC, IPTV, SAT>IP as input sources. Can be used as a backend
to Showtime, XBMC and various other clients.

%prep
%setup -q -n %{name}
#%patch999 -p1 -b .test

%build
echo %{version}-%{release} > %{_builddir}/%{name}/rpm/version
%ifarch i386 i686 
      %configure --disable-lockowner --enable-bundle --disable-libffmpeg_static
%else
      %configure --disable-lockowner --enable-bundle --enable-libffmpeg_static
%endif

%{__make}

%install
# binary
make install DESTDIR=%{buildroot}

# systemd stuff
mkdir -p -m755 %{buildroot}%{_sysconfdir}/sysconfig
install -p -m 644 rpm/tvheadend.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/tvheadend
mkdir -p -m755 %{buildroot}%{_unitdir}
install -p -m 644 rpm/tvheadend.service %{buildroot}%{_unitdir}

%pre
getent group tvheadend >/dev/null || groupadd -f -g 283 -r tvheadend
if ! getent passwd tvheadend > /dev/null ; then
  if ! getent passwd 283 > /dev/null ; then
    useradd -r -l -u 283 -g tvheadend -d /home/tvheadend -s /sbin/nologin -c "Tvheadend TV server" tvheadend
  else
    useradd -r -l -g tvheadend -d /home/tvheadend -s /sbin/nologin -c "Tvheadend TV server" tvheadend
  fi
  usermod -a -G wheel tvheadend
  usermod -a -G video tvheadend
  usermod -a -G audio tvheadend
fi
if ! test -d /home/tvheadend ; then
  mkdir -m 0755 /home/tvheadend || exit 1
  chown tvheadend.tvheadend /home/tvheadend || exit 1
fi
exit 0

%post
%systemd_post tvheadend.service

%postun
%systemd_postun_with_restart tvheadend.service

%files
%{_bindir}/*
%{_mandir}/*
%{_datadir}/%{name}/*
%{_sysconfdir}/sysconfig/*
%{_unitdir}/*

%changelog
* Fri Mar 20 2015 Bob Lightfoot <boblfoot@gmail.com> - 3.9.20150319-5
- added distribution tag to release for fedora/rhel differentiation
* Fri Mar 20 2015 Bob Lightfoot <boblfoot@gmail.com> - 3.9.20150319-4
- added conditionals for fedora/rhel and which ffmpeg to use
* Fri Mar 20 2015 Bob Lightfoot <boblfoot@gmail.com> - 3.9.20150319-3
- added ffmpeg-devel and release to rpm-version show in webui
* Fri Mar 20 2015 Bob Lightfoot <boblfoot@gmail.com> - 3.9.20150319-2
- switched to a no flags configure to make build multi arch compat
* Fri Mar 20 2015 Bob Lightfoot <boblfoot@gmail.com> - 3.9.20150319-1
- changed numbering to be consistent with upstream version of 3.9
* Thu Mar 19 2015 Bob Lightfoot <boblfoot@gmail.com> - 4.0.03192015
- packaged local git with web gui version fix for rpms
- bug 2720 fixed and submitted to upstream
* Wed Mar 18 2015 Bob Lightfoot <boblfoot@gmail.com> - 4.0.03182105
- repackaged to latest master branch commit
* Fri Feb 13 2015 Bob Lightfoot <boblfoot@gmail.com> - 4.0.02132015
- repackaged to latest master branch commit
* Thu Jan 01 2015 Bob Lightfoot <boblfoot@gmail.com> - 4.0.01012015
- Initial Packaging using modified upstream master rpm files
