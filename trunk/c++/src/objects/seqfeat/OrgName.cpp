/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'seqfeat.asn'.
 */

// standard includes

// generated includes
#include <objects/seqfeat/OrgName.hpp>

#include <objects/seqfeat/PartialOrgName.hpp>
#include <objects/seqfeat/TaxElement.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
COrgName::~COrgName(void)
{
}


bool COrgName::GetFlatName(string& name_out, string* lineage) const
{
    if (lineage  &&  lineage->empty()  &&  IsSetLineage()) {
        *lineage = GetLineage();
    }

    if ( !IsSetName() ) {
        return false;
    }

    const TName& name = GetName();
    switch (name.Which()) {
    case C_Name::e_Binomial: case C_Name::e_Namedhybrid:
    {
        const CBinomialOrgName& bin = (name.IsBinomial() ? name.GetBinomial()
                                       : name.GetNamedhybrid());
        name_out = bin.GetGenus();
        if (bin.IsSetSpecies()) {
            name_out += ' ' + bin.GetSpecies();
            if (bin.IsSetSubspecies()) {
                name_out += ' ' + bin.GetSubspecies();
            }
        }
        return true;
    }

    case COrgName::C_Name::e_Virus:
        name_out = name.GetVirus();
        return true;

    case COrgName::C_Name::e_Hybrid:
    {
        ITERATE (CMultiOrgName::Tdata, it, name.GetHybrid().Get()) {
            if ((*it)->GetFlatName(name_out, lineage)) {
                return true;
            }
        }
    }

    case COrgName::C_Name::e_Partial:
    {
        string delim;
        ITERATE (CPartialOrgName::Tdata, it, name.GetPartial().Get()) {
            name_out += delim + (*it)->GetName();
            delim = " ";
        }
        return true;
    }
    
    default:
        return false;
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/04/18 19:40:39  kans
* changed iterate to ITERATE
*
* Revision 1.1  2003/03/10 16:36:21  ucko
* +GetFlatName
*
*
* ===========================================================================
*/
/* Original file checksum: lines: 64, chars: 1877, CRC32: 38972243 */
