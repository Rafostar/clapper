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

%define debug_package %{nil}

%global appname com.github.rafostar.Clapper
%global gst_version 1.18.0
%global gtk4_version 4.0.0
%global meson_version 0.50
%global glib2_version 2.56.0

Name:           clapper
Version:        0.0.0
Release:        1%{?dist}
Summary:        Simple and modern GNOME media player

License:        GPL-3.0
URL:            https://github.com/Rafostar/clapper
BuildRoot:      %{_builddir}/%{name}-%{version}-build
Source0:        _service

BuildRequires:  meson >= %{meson_version}
BuildRequires:  gtk4-devel >= %{gtk4_version}
BuildRequires:  glib2-devel >= %{glib2_version}
BuildRequires:  gobject-introspection-devel
BuildRequires:  gjs
BuildRequires:  gcc-c++
BuildRequires:  desktop-file-utils
BuildRequires:  hicolor-icon-theme

Requires:       gjs
Requires:       gtk4 >= %{gtk4_version}
Requires:       hicolor-icon-theme

%if 0%{?suse_version}
# SUSE recommends group tag, while Fedora discourages their use
Group:          Productivity/Multimedia/Video/Players

BuildRequires:  update-desktop-files
BuildRequires:  gstreamer-devel >= %{gst_version}
BuildRequires:  gstreamer-plugins-base-devel >= %{gst_version}
BuildRequires:  Mesa-libGLESv2-devel
BuildRequires:  Mesa-libGLESv3-devel

Requires:       gstreamer >= %{gst_version}
Requires:       gstreamer-plugins-base >= %{gst_version}
Requires:       gstreamer-plugins-good >= %{gst_version}
Requires:       gstreamer-plugins-bad >= %{gst_version}

# Popular video decoders
Recommends:     gstreamer-plugins-libav >= %{gst_version}

# CD Playback
Suggests:       gstreamer-plugins-ugly
# Intel/AMD video acceleration
Suggests:       gstreamer-plugins-vaapi
%else
BuildRequires:  glibc-all-langpacks
BuildRequires:  gstreamer1-devel >= %{gst_version}
BuildRequires:  gstreamer1-plugins-base-devel >= %{gst_version}
BuildRequires:  mesa-libGL-devel
BuildRequires:  mesa-libGLES-devel
BuildRequires:  mesa-libGLU-devel
BuildRequires:  mesa-libEGL-devel

Requires:       gstreamer1 >= %{gst_version}
Requires:       gstreamer1-plugins-base >= %{gst_version}
Requires:       gstreamer1-plugins-good >= %{gst_version}
Requires:       gstreamer1-plugins-bad-free >= %{gst_version}

# ASS subtitles (assrender)
Recommends:     gstreamer1-plugins-bad-free-extras >= %{gst_version}

# CD Playback
Suggests:       gstreamer1-plugins-ugly-free
# Intel/AMD video acceleration
Suggests:       gstreamer1-vaapi
%endif

%description
A GNOME media player built using GJS with GTK4 toolkit and powered by GStreamer with OpenGL rendering.

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
%{_bindir}/%{appname}*
%{_datadir}/%{appname}/
%{_datadir}/icons/hicolor/*/apps/*.svg
%{_datadir}/glib-2.0/schemas/%{appname}.gschema.xml
%{_datadir}/mime/packages/%{appname}.xml
%{_datadir}/applications/*.desktop
%{_datadir}/metainfo/*.metainfo.xml
%{_datadir}/gir-1.0/GstClapper-1.0.gir
%{_libdir}/%{appname}/

%changelog
* Sun Feb 7 2021 Rafostar <rafostar.github@gmail.com> - 0.0.0-10
- Install gstclapper libs to app named subdirectory

* Fri Feb 5 2021 Rafostar <rafostar.github@gmail.com> - 0.0.0-9
- Update build with gstclapper libs support

* Thu Jan 21 2021 Rafostar <rafostar.github@gmail.com> - 0.0.0-8
- Use metainfo instead of deprecated appdata

* Mon Jan 18 2021 Rafostar <rafostar.github@gmail.com> - 0.0.0-7
- Remove gjs-1.0 files

* Sun Dec 20 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-6
- Include additional app binaries

* Sat Oct 31 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-5
- Added metainfo

* Sun Oct 25 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-4
- Added gschema

* Wed Oct 14 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-3
- Update to GTK4

* Sat Sep 19 22:02:00 CEST 2020 sp1rit - 0.0.0-2
- Added suse_update_desktop_file macro for SuSE packages

* Fri Sep 18 2020 Rafostar <rafostar.github@gmail.com> - 0.0.0-1
- Initial package
