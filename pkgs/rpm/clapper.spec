#
# spec file for package clapper
#
# Copyright (C) 2020  sp1rit
# Copyright (C) 2020  Rafostar
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


%global appname com.github.rafostar.Clapper
%global gst_version 1.18.0
%global gtk4_version 3.99.2

Name:           clapper
Version:        0.0.0
Release:        1%{?dist}
Summary:        Simple and modern GNOME media player

License:        GPL-3.0
URL:            https://github.com/Rafostar/clapper
BuildArch:      noarch
BuildRoot:      %{_builddir}/%{name}-%{version}-build
Source0:        _service

BuildRequires:  meson
BuildRequires:  gjs
BuildRequires:  desktop-file-utils
BuildRequires:  hicolor-icon-theme

Requires:       gjs
Requires:       gtk4 >= %{gtk4_version}
Requires:       hicolor-icon-theme

%if 0%{?suse_version}
# SUSE recommends group tag, while Fedora discourages their use
Group:          Productivity/Multimedia/Video/Players

BuildRequires:  update-desktop-files

Requires:       gstreamer
Requires:       gstreamer-plugins-base
Requires:       gstreamer-plugins-good
Requires:       gstreamer-plugins-good-gtk4
Requires:       gstreamer-plugins-bad
Requires:       libgstplayer-1_0-0 >= %{gst_version}

# Popular video decoders
Recommends:     gstreamer-plugins-libav

# CD Playback
Suggests:       gstreamer-plugins-ugly
# Intel/AMD video acceleration
Suggests:       gstreamer-plugins-vaapi
%else
BuildRequires:  glibc-all-langpacks
Requires:       gstreamer1
Requires:       gstreamer1-plugins-base
Requires:       gstreamer1-plugins-good
Requires:       gstreamer1-plugins-good-gtk4
# Contains GstPlayer lib
Requires:       gstreamer1-plugins-bad-free >= %{gst_version}

# ASS subtitles (assrender)
Recommends:     gstreamer1-plugins-bad-free-extras

# CD Playback
Suggests:       gstreamer1-plugins-ugly-free
# Intel/AMD video acceleration
Suggests:       gstreamer1-vaapi
%endif

%description
A GNOME media player built using GJS and powered by GStreamer with OpenGL rendering. Can also be used as a pre-made widget for GTK apps.

%prep
%setup -q -n %{_sourcedir}/%{name}-%{version} -T -D

%build
%meson
%meson_build

%install
%meson_install
%if 0%{?suse_version}
%suse_update_desktop_file %{appname}
%endif

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%files
%license COPYING
%doc README.md
%{_bindir}/%{appname}
%{_datadir}/%{appname}/
%dir %{_datadir}/gjs-1.0
%{_datadir}/gjs-1.0/*.js
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/apps/*.svg
%{_datadir}/mime/packages/%{appname}.xml

%changelog
* Wed Oct 14 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-3
- Update to GTK4

* Sat Sep 19 22:02:00 CEST 2020 sp1rit - 0.0.0-2
- Added suse_update_desktop_file macro for SuSE packages

* Fri Sep 18 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-1
- Initial package
