/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Paul Thiessen
*
* File Description:
*      Miscellaneous utility functions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2001/08/06 20:22:48  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.4  2001/08/03 13:41:24  thiessen
* add registry and style favorites
*
* Revision 1.3  2001/07/19 19:12:46  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.2  2001/05/31 18:46:26  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.1  2001/05/15 14:57:48  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* ===========================================================================
*/

#ifndef CN3D_TOOLS__HPP
#define CN3D_TOOLS__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <string>

class wxFrame;


BEGIN_SCOPE(Cn3D)

// strings for various directories (implemented in cn3d_main_wxwin.cpp)
extern const std::string& GetWorkingDir(void); // current working directory
extern const std::string& GetUserDir(void);    // directory of latest user-selected file
extern const std::string& GetProgramDir(void); // directory where Cn3D executable lives
extern const std::string& GetDataDir(void);    // 'data' directory with external data files

// bring the log window forward (implemented in cn3d_main_wxwin.cpp)
extern void RaiseLogWindow(void);

// launch web browser on given URL (implemented in sequence_set.cpp)
extern void LaunchWebPage(const char *url);

// global program registry (cn3d.ini) (implemented in cn3d_main_wxwin.cpp)
extern ncbi::CNcbiRegistry * GlobalRegistry(void);
extern void GlobalRegistryChanged(void);
static const std::string REG_CONFIG_SECTION = "Cn3D-4-Config";

// top-level window (the main structure window) (implemented in cn3d_main_wxwin.cpp)
extern wxFrame * GlobalTopWindow(void);

// diagnostic output (mainly for debugging)
#define TESTMSG(stream) ERR_POST(Info << stream)
//#define TESTMSG(stream)

// test for match between Sequence and Seq-id (implemented in alignment_set.cpp)
class Sequence;
extern bool IsAMatch(const Sequence *seq, const ncbi::objects::CSeq_id& sid);

END_SCOPE(Cn3D)

#endif // CN3D_TOOLS__HPP
