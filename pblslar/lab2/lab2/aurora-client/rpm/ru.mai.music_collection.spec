Name:       ru.mai.music_collection
Summary:    Offline music collection client for Aurora OS
Version:    1.0.0
Release:    4
License:    BSD-3-Clause
URL:        https://auroraos.ru
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  cmake
BuildRequires:  pkgconfig(auroraapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Sql)

%description
Qt/QML/C++ HTTPS REST client with SQLite offline cache and synchronization.

%prep
%autosetup

%build
%cmake
%make_build

%install
%make_install

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%defattr(644,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Tue Oct 13 2026 Student <student@example.com> - 1.0.0-1
- Initial package for laboratory work 2.
