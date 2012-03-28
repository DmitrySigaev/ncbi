Name:        ncbi-igblast
Version:     BLAST_VERSION
Release:     1
Source0:     %{name}-%{version}.tgz
Summary:     NCBI igBLAST finds regions of similarity between biological sequences. 
Exclusiveos: linux
Group:       NCBI/BLAST
License:     Public Domain
BuildArch:   i686 x86_64
BuildRoot:   /var/tmp/%{name}-buildroot
Prefix:      /usr

%description
Immunoglobulin BLAST (IgBLAST), a specialized variant of BLAST that is designed
for the analysis of immunoglobulin sequence data.

%prep 
%setup -q

%build
./configure
cd c++/*/build
%__make -f Makefile.flat

%install
%__mkdir_p $RPM_BUILD_ROOT/%_bindir
cd c++/*/bin
%__install -m755 igblastp igblastn $RPM_BUILD_ROOT/%_bindir

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%_bindir/*

%changelog
* Tue Mar 20 2012 Christiam Camacho <camacho@ncbi.nlm.nih.gov>
- See ChangeLog file

