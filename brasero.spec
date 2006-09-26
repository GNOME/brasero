Name:		brasero
Version:		0.4.91
Release:		1%{?dist}
Summary:		Gnome CD/DVD burning application

Group:		Applications/Multimedia
License:		GPL
URL:		http://perso.wanadoo.fr/%{name}
Source0:		http://download.sourceforge.net/%{name}/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	gettext
BuildRequires:	intltool			>= 0.22
BuildRequires:	desktop-file-utils

BuildRequires:	libgnomeui-devel	>= 2.14
BuildRequires:	gstreamer-devel	>= 0.10.6
BuildRequires:	gstreamer-plugins-base-devel
BuildRequires:	totem-devel		>= 1.4.0
BuildRequires:	libnotify-devel		>= 0.3.0
BuildRequires:	libbeagle-devel		>= 0.2.5
BuildRequires:	nautilus-cd-burner-devel	>= 2.14.0

Requires:		dvd+rw-tools
Requires:		cdrecord
Requires:		mkisofs
Requires:		cdda2wav
Requires:		cdrdao
Requires:		gtk2 >= 2.8.0

Requires(post):		shared-mime-info
Requires(postun):	shared-mime-info
Requires(post):		desktop-file-utils
Requires(postun):	desktop-file-utils

%description
Simple and easy to use CD/DVD burning application for the gnome desktop


%prep
%setup -q
chmod 644 AUTHORS
chmod 644 brasero.spec


%build
%configure --disable-rpath --disable-caches --disable-schemas-install
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
%find_lang %{name}

# Remove duplicate docs.
rm -rf $RPM_BUILD_ROOT/usr/doc/%{name}/

export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
make install DESTDIR=${RPM_BUILD_ROOT}
unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL

desktop-file-install --vendor fedora --delete-original	\
	--dir $RPM_BUILD_ROOT%{_datadir}/applications   	\
	--add-category X-Fedora			        \
	$RPM_BUILD_ROOT%{_datadir}/applications/%{name}.desktop


%clean
rm -rf $RPM_BUILD_ROOT


%post
update-mime-database %{_datadir}/mime &> /dev/null || :
touch --no-create %{_datadir}/icons/gnome || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
	%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/gnome || :
fi
update-desktop-database &> /dev/null ||:
GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source` \
  gconftool-2 --makefile-install-rule %{_sysconfdir}/gconf/schemas/%{name}.schemas &>/dev/null || :

%preun
if [ "$1" = "0" ] ; then
  GCONF_CONFIG_SOURCE=`gconftool-2 --get-default-source` \
    gconftool-2 --makefile-uninstall-rule %{_sysconfdir}/gconf/schemas/%{name}.schemas &>/dev/null || :
fi

%postun
update-mime-database %{_datadir}/mime &> /dev/null || :
touch --no-create %{_datadir}/icons/gnome || :
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
	%{_bindir}/gtk-update-icon-cache --quiet %{_datadir}/icons/gnome || :
fi
update-desktop-database &> /dev/null ||:

%files -f %{name}.lang
%defattr(-,root,root,-)
%doc AUTHORS COPYING ChangeLog NEWS README
%{_bindir}/*
%{_datadir}/%{name}
%{_datadir}/applications/fedora-brasero.desktop
%{_datadir}/pixmaps/*
%{_datadir}/icons/gnome/48x48/mimetypes/*
%{_datadir}/mime/packages/*
%{_mandir}/man[^3]/*
%{_sysconfdir}/gconf/schemas/%{name}.schemas

%changelog
* Wed Sep 6 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.4.91-1
- updated for 0.4.91 release
* Wed Sep 6 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.4.90-1
- updated for 0.4.90 release
* Mon Jul 10 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.4.0-1
- updated for 0.4.0 release
* Fri Jun 28 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.3.91-1
- updated for 0.3.91 release
* Wed Jun 13 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.3.90-1
- corrected brasero.spec and updated for 0.3.90 release
* Fri May 12 2006 Rouquier Philippe <bonfire-app@wanadoo.fr> - 0.3.1-1
- Initial release of spec file.
