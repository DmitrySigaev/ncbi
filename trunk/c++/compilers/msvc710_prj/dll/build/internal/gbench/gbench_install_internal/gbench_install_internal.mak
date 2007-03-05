# $Id$
#################################################################
#
# Main Installation makefile for NCBI Internal Plugins
#
# Author: Mike DiCuccio
#
#################################################################



#################################################################
#
# Overridable variables
# These can be modified on the NMAKE command line
#

#
# Tag defining prefixes for a number of paths
MSVC = msvc7

#
# This is the main path for installation.  It can be overridden
# on the command-line
INSTALL       = ..\..\..\..\bin



#################################################################
#
# Do not override any of the variables below on the NMAKE command
# line
#

!IF "$(INTDIR)" == ".\DebugDLL"
INTDIR = DebugDLL
!ELSEIF "$(INTDIR)" == ".\ReleaseDLL"
INTDIR = ReleaseDLL
!ENDIF

#
# Alias for the bin directory
#
DLLBIN        = $(INSTALL)\$(INTDIR)

#
# Alias for the genome workbench path
#
GBENCH        = $(DLLBIN)\gbench

#
# Third-party DLLs' installation path and rules
#
INSTALL_BINPATH          = $(GBENCH)\bin
THIRDPARTY_MAKEFILES_DIR =  ..\..\..\..
STAMP_SUFFIX             = _gbench
!include $(THIRDPARTY_MAKEFILES_DIR)\Makefile.mk


#
# Alias for the source tree
#
SRCDIR        = ..\..\..\..\..\..\..\src

#
# Alias for the local installation of third party libs
#
THIRDPARTYLIB = ..\..\..\..\..\..\..\..\lib\$(INTDIR)


#
# Internal Plugins
#
INTERNAL_PLUGINS = \
        proteus \
        radar \
        contig

#
#
# Main Targets
#
all : dirs \
    $(INTERNAL_PLUGINS)

clean :
    @echo Removing Genome Workbench Installation...
    -@rmdir /S /Q $(GBENCH)\extra
    -@del $(GBENCH)\..\..\..\$(INTDIR)\fltk_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\berkeleydb_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\sqlite_gbench.installed
    -@del $(GBENCH)\..\..\..\$(INTDIR)\msvc_gbench.installed


###############################################################
#
# Target: Create our directory structure
#
dirs :
    @echo Creating directory structure...
    @if not exist $(GBENCH) mkdir $(GBENCH)
    @if not exist $(GBENCH)\extra mkdir $(GBENCH)\extra 


###############################################################
#
# Target: internal plugins
#

proteus :
    @echo Installing Proteus...
    @if not exist $(GBENCH)\extra\proteus mkdir $(GBENCH)\extra\proteus
    @if exist $(DLLBIN)\proteus.dll copy $(DLLBIN)\proteus.dll $(GBENCH)\extra\proteus\proteus.dll
    @if exist $(DLLBIN)\proteus.pdb copy $(DLLBIN)\proteus.pdb $(GBENCH)\extra\proteus\proteus.pdb
    @if exist $(SRCDIR)\internal\gbench\plugins\proteus\Proteus\proteus-config.asn copy $(SRCDIR)\internal\gbench\plugins\proteus\Proteus\proteus-config.asn $(GBENCH)\extra\proteus\proteus-config.asn
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\extra\proteus

radar :
    @echo Installing Radar...
    @if not exist $(GBENCH)\extra\radar mkdir $(GBENCH)\extra\radar
    @if exist $(DLLBIN)\radar.dll copy $(DLLBIN)\radar.dll $(GBENCH)\extra\radar\radar.dll
    @if exist $(DLLBIN)\radar.pdb copy $(DLLBIN)\radar.pdb $(GBENCH)\extra\radar\radar.pdb
    @if exist $(DLLBIN)\dload_alignmodel.dll copy $(DLLBIN)\dload_alignmodel.dll $(GBENCH)\extra\radar\dload_alignmodel.dll
    @if exist $(DLLBIN)\dload_alignmodel.pdb copy $(DLLBIN)\dload_alignmodel.pdb $(GBENCH)\extra\radar\dload_alignmodel.pdb
    @if exist $(DLLBIN)\view_radar.dll copy $(DLLBIN)\view_radar.dll $(GBENCH)\extra\radar\view_radar.dll
    @if exist $(DLLBIN)\view_radar.pdb copy $(DLLBIN)\view_radar.pdb $(GBENCH)\extra\radar\view_radar.pdb
    @if exist $(SRCDIR)\internal\gbench\plugins\radar\plugin\radar-config.asn copy $(SRCDIR)\internal\gbench\plugins\radar\plugin\radar-config.asn $(GBENCH)\extra\radar\radar-config.asn
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\extra\radar

contig :
    @echo Installing Contig...
    @if not exist $(GBENCH)\extra\contig mkdir $(GBENCH)\extra\contig
    @if exist $(DLLBIN)\dload_contig.dll copy $(DLLBIN)\dload_contig.dll $(GBENCH)\extra\contig\dload_contig.dll
    @if exist $(DLLBIN)\dload_contig.pdb copy $(DLLBIN)\dload_contig.pdb $(GBENCH)\extra\contig\dload_contig.pdb
    @if exist $(SRCDIR)\internal\gbench\plugins\contig\contig-config.asn copy $(SRCDIR)\internal\gbench\plugins\contig\contig-config.asn $(GBENCH)\extra\contig\contig-config.asn
    @$(GBENCH)\bin\gbench_plugin_scan -strict $(GBENCH)\extra\contig
