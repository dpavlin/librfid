Name:           librfid
Version:        0.1.0 
Release:        1%{?dist}
Summary:        The librfid is a Free Software RFID library

Group:          System Environment/Libraries
License:        GPL
URL:            http://www.openmrtd.org/projects/librfid/
Source0:        http://www.openmrtd.org/projects/librfid/files/%{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  libusb-devel


%description
librfid is a Free Software RFID library. It implements the PCD (reader) 
side protocol stack of ISO 14443 A, ISO 14443 B, ISO 15693, 
Mifare Ultralight and Mifare Classic. Support for iCODE and 
other 13.56MHz based transponders is planned.


%package        devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%prep
%setup -q


%build
%configure --disable-static
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'


%clean
rm -rf $RPM_BUILD_ROOT


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc COPYING README TODO
%{_libdir}/*.so.*
%{_bindir}/librfid-tool
%{_bindir}/mifare-tool
%{_bindir}/send_script

%files devel
%defattr(-,root,root,-)
%doc COPYING README TODO
%{_includedir}/*
%{_libdir}/*.so


%changelog
* Sat Dec 30 2006 Kushal Das <kushal@openpcd.org> 0.1.0-1
- Initial release
