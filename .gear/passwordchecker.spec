%define gui_name PasswordCheckerSettings

Name:    passwordchecker
Version: 0.0.1
Release: alt1

Summary: Password expiry notification application
License: GPLv3
Group:   Other
Url:     https://github.com/alxvmr/passwordchecker

BuildRequires(pre): rpm-macros-cmake
BuildRequires: ccmake gcc-c++
BuildRequires: pkgconfig(gio-2.0) pkgconfig(gtk4)
BuildRequires: libldap-devel libsasl2-devel

Source0: %name-%version.tar

%description
Daemon with GUI settings for password expiration notification

%prep
%setup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%_bindir/%name
%_bindir/%gui_name
%_datadir/%gui_name
%_user_unitdir/%name-user.service
%_datadir/glib-2.0/schemas/org.altlinux.%name.gschema.xml

%changelog
* Thu Mar 06 2025 Maria Alexeeva <alxvmr@altlinux.org> 0.0.1-alt1
- Init build

