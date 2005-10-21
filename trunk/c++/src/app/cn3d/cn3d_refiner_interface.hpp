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
* Authors:  Chris Lanczycki
*
* File Description:
*      Interface to blocked multiple alignment refiner algorithm
*
* ===========================================================================
*/

#ifndef CN3D_REFINER_INTERFACE__HPP
#define CN3D_REFINER_INTERFACE__HPP

#include <corelib/ncbistl.hpp>

#include <list>

class wxWindow;

//  Forward-declare refiner engine class
BEGIN_SCOPE(align_refine)
class CBMARefinerEngine;
END_SCOPE(align_refine)

BEGIN_SCOPE(struct_util)
class AlignmentUtility;
END_SCOPE(struct_util)


BEGIN_SCOPE(Cn3D)

class BMARefiner
{
public:
    BMARefiner(void);
    ~BMARefiner(void);

    typedef std::list < struct_util::AlignmentUtility * > AlignmentUtilityList;
    typedef AlignmentUtilityList::iterator AlignmentUtilityListIt;

    // Refines an initial blocked multiple alignment according to parameters in
    // a dialog filled in by user.  All non-master rows are refined unless user
    // declares otherwise.  The output AlignmentList contains the results
    // ordered from best to worst refined score (refiner engine 'owns' the alignments).
    // If the refinement alignment algorithm fails, a null element is appended
    // to the list.  Return 'false' only if all trials fail.
    bool RefineMultipleAlignment(struct_util::AlignmentUtility *originalMultiple,
        AlignmentUtilityList *refinedMultiples, wxWindow *parent);

    //  In case it's desired to get info results/settings directly from the refiner engine.
    const align_refine::CBMARefinerEngine* GetRefiner() {return m_refinerEngine;}

private:

    align_refine::CBMARefinerEngine* m_refinerEngine;

    // brings up a dialog that lets the user set refinement parameters; returns false if cancelled
    // initializes the refiner engine w/ values found in the dialog.
    bool ConfigureRefiner(wxWindow* parent, unsigned int nAlignedBlocks);
};

END_SCOPE(Cn3D)

#endif // CN3D_REFINER_INTERFACE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2005/10/21 21:59:49  thiessen
* working refiner integration
*
* Revision 1.1  2005/10/18 21:38:33  lanczyck
* initial versions (still containing CJL hacks incompatible w/ official cn3d)
*
* ---------------------------------------------------------------------------
*/
