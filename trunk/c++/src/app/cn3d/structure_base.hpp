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
*      Base class for objects that will hold structure data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/07/27 13:30:10  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.7  2000/07/16 23:18:34  thiessen
* redo of drawing system
*
* Revision 1.6  2000/07/12 23:28:28  thiessen
* now draws basic CPK model
*
* Revision 1.5  2000/07/11 13:49:29  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.4  2000/07/01 15:44:23  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.3  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:16  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:08:13  thiessen
* initial checkin
*
* ===========================================================================
*/

#ifndef CN3D_STRUCTUREBASE__HPP
#define CN3D_STRUCTUREBASE__HPP

// container type used for various lists
#include <list>
#define LIST_TYPE std::list

#include <map>
#include <corelib/ncbidiag.hpp>

#define TESTMSG(stream) ERR_POST(Info << stream)
//#define TESTMSG(stream)


BEGIN_SCOPE(Cn3D)

class StructureBase
{
public:
    // will store StructureBase-derived children in parent upon construction
    StructureBase(StructureBase *parent, bool warnOnNULL = true);

    // will automatically delete children upon destruction with this desctuctor.
    // But note that derived classes can have their own constructors that will
    // *also* be called upon deletion; derived class destructors should *not*
    // delete any StructureBase-derived objects.
    virtual ~StructureBase(void);

    // overridable default Draws the object and all its children; 'data' will
    // be passed along to self and children's Draw methods
    virtual bool DrawAll(const StructureBase *data = NULL) const;

    // function to draw this object, called before children are Drawn. Return
    // false to halt recursive drawing process
    virtual bool Draw(const StructureBase *data = NULL) const { return true; }

private:
    // no default construction
    StructureBase(void);

    // keep track of StructureBase-derived children, so that top-down operations
    // like drawing or deconstructing can trickle down automatically
    typedef std::map < StructureBase * , int > _ChildList;
    _ChildList _children;
    void _AddChild(StructureBase *child);
    void _RemoveChild(StructureBase *child);
    StructureBase* _parent;

public:
    // go up the hierarchy to find a parent of the desired type
    template < class T >
    bool GetParentOfType(T* *ptr) const
    {
        *ptr = NULL;
        for (const StructureBase *parent=this->_parent; parent; parent=parent->_parent) {
            if ((*ptr = dynamic_cast<T*>(parent)) != NULL) {
                return true;
            }
        }
        return false;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTUREBASE__HPP
