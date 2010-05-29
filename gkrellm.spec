%define name gkrellm
%define version 2.3.4
%define release 1
%define prefix /usr

Summary:	Multiple stacked system monitors: 1 process.
Name:	%{name}
Version:	%{version}
Release:	%{release}
License:	GPL
Group:	X11/Utilities
URL:	http://gkrellm.net
Source:	http://members.dslextreme.com/users/billw/gkrellm/%{name}-%{version}.tar.bz2
Vendor:	Bill Wilson <billw--at--gkrellm.net>
Packager:	Bill Wilson <billw--at--gkrellm.net>
Requires:	gtk2 >= 2.4, glib2 >= 2.0
BuildRoot:	%{_tmppath}/%{name}-%{version}-root

%description
GKrellM charts SMP CPU, load, Disk, and all active net interfaces
automatically. An on/off button and online timer for the PPP interface
is provided. Monitors for memory and swap usage, file system, internet
connections, APM laptop battery, mbox style mailboxes, and cpu temps.
Also includes an uptime monitor, a hostname label, and a clock/calendar.
Additional features are:

  * Autoscaling grid lines with configurable grid line resolution.
  * LED indicators for the net interfaces.
  * A gui popup for configuration of chart sizes and resolutions.

%prep

%setup -q

%build
make CFLAGS="$RPM_OPT_FLAGS" LOCALEDIR="%{_datadir}/locale"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
mkdir -p $RPM_BUILD_ROOT%{_libdir}/pkgconfig  
mkdir -p $RPM_BUILD_ROOT%{_includedir}/%{name}
make install INSTALLROOT=$RPM_BUILD_ROOT/%{_prefix} LOCALEDIR=$RPM_BUILD_ROOT%{_datadir}/locale MANDIR=$RPM_BUILD_ROOT/%{_mandir}/man1

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYRIGHT Changelog README Themes.html
%doc INSTALL
%{_bindir}/gkrellm
%{_bindir}/gkrellmd
%{_mandir}/man1/gkrellm.1.gz
%{_mandir}/man1/gkrellmd.1.gz
%{_includedir}/gkrellm2/gkrellm*
%{_includedir}/gkrellm2/log*
%{_libdir}/pkgconfig/gkrellm.pc
%{_datadir}/locale/*/LC_MESSAGES/gkrellm.mo

%changelog
* Sat Jun  7 2003 Henrik Brix Andersen <brix@gimp.org>
- changed Source from .gz to .bz2
- added prefix to install path

* Sun Jun 1 2003 Bill Wilson <billw@wt.net>
- Use INSTALLROOT and added pkgconfig lines.

* Wed Aug 14 2002 Vladimir Kondratiev <vladimir.kondratiev@intel.com>
- spec: include locale, server files, change 'include' subdir name

* Tue May 1 2001 Vladimir Kondratiev <vladimir.kondratiev@intel.com>
- use macros like _bindir, _mandir, _includedir
- source 1.0.8

* Wed Mar 14 2001 Rob Lineweaver <rbline@wm.edu>
- fixed new manpage inclusion for newer RPM versions
- source is 1.0.7
- compiled for PPC and i386

* Fri Jan 19 2001 Kevin Ford <klford@uitsg.com>
- general cleanup of spec file

* Thu Jan 18 2001 Kevin Ford <klford@uitsg.com>
- Updated spec file to work with both v3 & v4 rpm
- moved changelog to bottom of spec file
- added defines for common items

* Thu Apr 6 2000 Bill Wilson
- added INCLUDEDIR to the make install

* Fri Oct 29 1999 Gary Thomas <gdt@linuxppc.org>
- .spec file still broken

* Thu Oct 7 1999 David Mihm <davemann@ionet.net>
- fixed spec.

