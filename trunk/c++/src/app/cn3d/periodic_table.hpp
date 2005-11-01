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
*      Classes to information about atomic elements
*
* ===========================================================================
*/

#ifndef CN3D_PERIODICTABLE__HPP
#define CN3D_PERIODICTABLE__HPP

#include <corelib/ncbistl.hpp>

#include <map>

#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Element
{
public:
  const char *name, *symbol;
  double vdWRadius;
  Vector color;

  Element(const char *n, const char *s,
          double r, double g, double b, double v) :
    name(n), symbol(s), vdWRadius(v), color(r,g,b) { }
};

class PeriodicTableClass
{
private:
  typedef std::map < int, const Element * > ZMapType;
  ZMapType ZMap;

public:
  PeriodicTableClass(void);
  ~PeriodicTableClass(void);

  const Element * GetElement(int Z) const;
  void AddElement(int Z, const char * name,
                  const char * symbol,
                  double r, double g, double b,
                  double vdW);
};

extern PeriodicTableClass PeriodicTable; // one global copy for now


// general utility functions

inline bool IsMetal(int atomicNumber)
{
    if ((atomicNumber >= 3 && atomicNumber <= 4) ||      // Li..Be
        (atomicNumber >= 11 && atomicNumber <= 13) ||    // Na..Al
        (atomicNumber >= 19 && atomicNumber <= 32) ||    // K..Ge
        (atomicNumber >= 37 && atomicNumber <= 51) ||    // Rb..Sb
        (atomicNumber >= 55 && atomicNumber <= 84) ||    // Cs..Po
        (atomicNumber >= 87 && atomicNumber <= 109))     // Fr..Mt
        return true;

    return false;
}

END_SCOPE(Cn3D)

#endif // CN3D_PERIODICTABLE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2005/11/01 02:44:07  thiessen
* fix GCC warnings; switch threader to C++ PSSMs
*
* Revision 1.7  2005/10/19 17:28:19  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.6  2004/02/19 17:05:02  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.5  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.4  2000/10/04 17:40:46  thiessen
* rearrange STL #includes
*
* Revision 1.3  2000/08/24 18:43:15  thiessen
* tweaks for transparent sphere display
*
* Revision 1.2  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.1  2000/07/12 23:30:47  thiessen
* now draws basic CPK model
*
*/
