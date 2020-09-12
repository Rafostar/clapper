#
# spec file for package clapper
#
# Copyright (C) 2020  sp1rit
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


Name:				clapper
Version:			0.0.0
Release:			0
Summary:			A GNOME media player
License:			GPL-3.0
URL:				https://github.com/Rafostar/clapper
BuildArchitectures:		noarch
BuildRoot:			%{_builddir}/%{name}-%{version}-build
BuildRequires:			meson gjs
Requires:			gjs gstreamer
Source0:			_service
%if 0%{?suse_version}
Requires:			gstreamer-plugins-base gstreamer-plugins-good-gtk libgstplayer-1_0-0
Recommends:			gstreamer-plugins-vaapi
%endif
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:		glibc-all-langpacks
Requires:			gstreamer1-plugins-base gstreamer1-plugins-good-gtk gstreamer1-plugins-bad-free
Recommends:			gstreamer1-vaapi
%endif

%description
A GNOME media player built using GJS and powered by GStreamer with OpenGL rendering. Can also be used as a pre-made widget for Gtk apps.

%prep
%setup -q -n %_sourcedir/%name-%version -T -D

%build
%meson
%meson_build

%install
%meson_install
ln -s %{_bindir}/com.github.rafostar.Clapper %{buildroot}/%{_bindir}/clapper

%files
%license COPYING
%doc README.md
%_bindir/com.github.rafostar.Clapper
%_bindir/clapper
%_datadir/com.github.rafostar.Clapper/
%dir %_datadir/gjs-1.0/
%_datadir/gjs-1.0/clapper.js

%changelog
