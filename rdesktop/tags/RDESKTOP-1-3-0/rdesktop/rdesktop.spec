Summary: Remote Desktop
Name: rdesktop
Version: 1.3.0
Release: 1
Copyright: GPL; see COPYING
Group: Applications/Communications
Source: rdesktop.tgz
BuildRoot: %{_tmppath}/%{name}-buildroot
Packager: Peter �strand <peter@cendio.se>
Requires: XFree86-libs 

%description
rdesktop is a client for Remote Desktop Protocol (RDP), used in a number of
Microsoft products including Windows NT Terminal Server, Windows 2000 Server,
Windows XP and Windows 2003 Server.

%prep
rm -rf $RPM_BUILD_ROOT

%setup -n rdesktop
%build 
./configure --prefix=%{_prefix} --bindir=%{_bindir} --mandir=%{_mandir}
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING doc/AUTHORS doc/keymapping.txt doc/keymap-names.txt doc/ipv6.txt
%{_bindir}/rdesktop
%{_mandir}/man1/rdesktop.1*
%{_datadir}/rdesktop/keymaps

%post

%postun

%clean
rm -rf $RPM_BUILD_ROOT

