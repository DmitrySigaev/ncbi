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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.1  2000/07/12 23:30:47  thiessen
* now draws basic CPK model
*
* ===========================================================================
*/

#ifndef CN3D_PERIODICTABLE__HPP
#define CN3D_PERIODICTABLE__HPP

#include <map>

#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Element
{
public:
  const char *name, *symbol;
  double vdWRadius;
  Vector color;

  Element(const char *n, const char *s,
          double r, double g, double b, double v) :
    name(n), symbol(s), color(r,g,b), vdWRadius(v) { }
};

class PeriodicTableClass
{
private:
  typedef std::map < int, const Element * > ZMapType;
  ZMapType ZMap;

public:
  PeriodicTableClass(void);
  ~PeriodicTableClass(void);

  const Element * GetElement(int) const;
  void AddElement(int Z, const char * name,
                  const char * symbol,
                  double r, double g, double b,
                  double vdW);
};

extern PeriodicTableClass PeriodicTable; // one global copy for now

END_SCOPE(Cn3D)

#endif // CN3D_PERIODICTABLE__HPP
