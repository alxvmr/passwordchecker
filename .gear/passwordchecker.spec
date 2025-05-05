%define _unpackaged_files_terminate_build 1
%define gui_name PasswordCheckerSettings
%define build_dir_adwaita build_adwaita
%define build_dir_gtk build_gtk
%global uuid org.altlinux.PasswordCheckerSettings

Name:    passwordchecker
Version: 0.0.1
Release: alt1

Summary: Password expiry notification application
License: GPLv3
Group:   System/Configuration/Other
Url:     https://github.com/alxvmr/passwordchecker

BuildRequires(pre): rpm-macros-cmake
BuildRequires: cmake gcc gettext-tools
BuildRequires: pkgconfig(gio-2.0) pkgconfig(wbclient)
BuildRequires: libldap-devel libsasl2-devel libwbclient-devel
Requires: samba-winbind

Source0: %name-%version.tar

%description
Daemon for password expiration notification

%package -n %gui_name-common
Summary: Translation files, .desktop and icons for PasswordCheckerSettings
Group: System/Configuration/Other
BuildArch: noarch

%description -n %gui_name-common
%summary

%package -n %gui_name-gtk
Summary: A application on GTK4 to customize notification settings
Group: System/Configuration/Other
BuildRequires: pkgconfig(gtk4)
Requires: %gui_name-common %name

%description -n %gui_name-gtk
%summary

%package -n %gui_name-gnome
Summary: A application on Adwaita to customize notification settings
Group: System/Configuration/Other
BuildRequires: pkgconfig(gtk4) pkgconfig(libadwaita-1)
Requires: %gui_name-common %name

%description -n %gui_name-gnome
%summary

%prep
%setup
mkdir -p %build_dir_adwaita
mkdir -p %build_dir_gtk

%build
%cmake -B %build_dir_adwaita\
    -DUSE_ADWAITA=ON
%cmake -B %build_dir_gtk\
    -DUSE_ADWAITA=OFF

cmake --build %build_dir_adwaita -j%__nprocs
cmake --build %build_dir_gtk -j%__nprocs

%install
DESTDIR=%buildroot cmake --install %build_dir_adwaita
# rename PasswordCheckerSettings (adwaita) -> PasswordCheckerSettings-adwaita
mv %buildroot%_bindir/%gui_name  %buildroot%_bindir/%gui_name-adwaita

DESTDIR=%buildroot cmake --install %build_dir_gtk
# rename PasswordCheckerSettings (gtk) -> PasswordCheckerSettings-gtk
mv %buildroot%_bindir/%gui_name  %buildroot%_bindir/%gui_name-gtk

mkdir -p %buildroot/%_altdir
cat >%buildroot/%_altdir/%gui_name-adwaita <<EOF
%_bindir/%gui_name    %_bindir/%gui_name-adwaita    50
EOF

mkdir -p %buildroot/%_altdir
cat >%buildroot/%_altdir/%gui_name-gtk <<EOF
%_bindir/%gui_name    %_bindir/%gui_name-gtk    30
EOF

%find_lang %name

%files
%_bindir/%name
%_user_unitdir/%name-user.service
%_datadir/glib-2.0/schemas/org.altlinux.%name.gschema.xml

%files -n %gui_name-gtk
%_bindir/%gui_name-gtk
%_altdir/%gui_name-gtk

%files -n %gui_name-gnome
%_bindir/%gui_name-adwaita
%_altdir/%gui_name-adwaita

%files -n %gui_name-common -f %name.lang
%_datadir/%gui_name
%_desktopdir/%uuid.desktop

%changelog
* Thu Mar 06 2025 Maria Alexeeva <alxvmr@altlinux.org> 0.0.1-alt1
- Init build

