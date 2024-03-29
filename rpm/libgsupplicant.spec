Name: libgsupplicant

Version: 1.0.27
Release: 0
Summary: Client library for wpa_supplicant
License: BSD
URL: https://github.com/sailfishos/libgsupplicant
Source: %{name}-%{version}.tar.bz2

%define libglibutil_version 1.0.52

BuildRequires: pkgconfig
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(libglibutil) >= %{libglibutil_version}

# license macro requires rpm >= 4.11
BuildRequires: pkgconfig(rpm)
%define license_support %(pkg-config --exists 'rpm >= 4.11'; echo $?)

Requires: libglibutil >= %{libglibutil_version}
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Provides glib-based wpa_supplicant client API

%package devel
Summary: Development library for %{name}
Requires: %{name} = %{version}

%description devel
This package contains the development library for %{name}.

%prep
%setup -q

%build
make %{_smp_mflags} LIBDIR=%{_libdir} KEEP_SYMBOLS=1 release pkgconfig

%install
rm -rf %{buildroot}
make LIBDIR=%{_libdir} DESTDIR=%{buildroot} install-dev

%check
make -C test test

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/%{name}.so.*
%if %{license_support} == 0
%license LICENSE
%endif

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/*.pc
%{_libdir}/%{name}.so
%{_includedir}/gsupplicant/*.h
