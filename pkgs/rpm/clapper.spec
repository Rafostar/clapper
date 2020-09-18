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
%global gst_version 1.16.0
%global gtk3_version 3.19.4

Name:           clapper
Version:        0.0.0
Release:        1
Summary:        Simple and modern GNOME media player

License:        GPL-3.0
URL:            https://github.com/Rafostar/clapper
BuildArch:      noarch
BuildRoot:      %{_builddir}/%{name}-%{version}-build
Source0:        _service

BuildRequires:  meson
BuildRequires:  gjs

Requires:       gjs
Requires:       gtk3 >= %{gtk3_version}
Requires:       hicolor-icon-theme

%if 0%{?suse_version}
Requires:       gstreamer
Requires:       gstreamer-plugins-base
Requires:       gstreamer-plugins-good-gtk
Requires:       gstreamer-plugins-bad
Requires:       libgstplayer-1_0-0 >= %{gst_version}

Recommends:     gstreamer-plugins-libav             # Popular video decoders

Suggests:       gstreamer-plugins-ugly              # CD Playback
Suggests:       gstreamer-plugins-vaapi             # Intel/AMD video acceleration
%endif

%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
BuildRequires:  glibc-all-langpacks
Requires:       gstreamer1
Requires:       gstreamer1-plugins-base
Requires:       gstreamer1-plugins-good-gtk
Requires:       gstreamer1-plugins-bad-free >= %{gst_version} # Contains GstPlayer lib

Recommends:     gstreamer1-plugins-bad-free-extras  # ASS subtitles (assrender)

Suggests:       gstreamer1-plugins-ugly-free        # CD Playback
Suggests:       gstreamer1-vaapi                    # Intel/AMD video acceleration
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
ln -s %{_bindir}/%{appname} %{buildroot}/%{_bindir}/%{name}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop

%files
%license COPYING
%doc README.md
%{_bindir}/%{appname}
%{_bindir}/%{name}
%{_datadir}/%{appname}/
%{_datadir}/gjs-1.0/*.js
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/apps/*.svg
%{_datadir}/mime/packages/%{appname}.xml

%changelog
