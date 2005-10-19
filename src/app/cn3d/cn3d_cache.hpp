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
*      implements a basic cache for Biostrucs
*
* ===========================================================================
*/

#ifndef CN3D_CACHE__HPP
#define CN3D_CACHE__HPP

#include <corelib/ncbistd.hpp>

#include <string>
#include <list>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/seq/Bioseq.hpp>


BEGIN_SCOPE(Cn3D)

// Retrieves a Biostruc from the cache, according to the cache configuration currently in the registry;
// returns true on success or false if fetch fails for any reason. MMDB entries are cached according to
// asn Model-type, so an ncbi-backbone and pdb-model for a given MMDB ID are cached independently.
// If the cache is disabled, this will always attempt to load via network. (The contents of
// the passed *biostruc are deleted and replaced by the fetched data.) modelType can be:
//   eModel_type_ncbi_backbone (alpha only),
//   eModel_type_ncbi_all_atom (one coordinate per atom),
//   eModel_type_ncbi_pdb_model (all models from PDB, including alternate conformer ensembles)
// If sequences is not NULL, the list will be filled with Bioseqs for all biopolymer chains
// in the structure.

typedef std::list < ncbi::CRef < ncbi::objects::CBioseq > > BioseqRefList;

// uid can either be an integer MMDB ID or a 4-character PDB ID
bool LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType,
    ncbi::CRef < ncbi::objects::CBiostruc >& biostruc, BioseqRefList *sequences);
ncbi::objects::CNcbi_mime_asn1 *
    LoadStructureViaCache(const std::string& uid, ncbi::objects::EModel_type modelType);

// utility function for mimes
bool ExtractBiostrucAndBioseqs(ncbi::objects::CNcbi_mime_asn1& mime,
    ncbi::CRef < ncbi::objects::CBiostruc >& biostruc, BioseqRefList *sequences);

// Remove older entries until the cache is <= the given size (in MB).

void TruncateCache(unsigned int maxSize);

END_SCOPE(Cn3D)

#endif // CN3D_CACHE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.6  2004/01/17 00:17:29  thiessen
* add Biostruc and network structure load
*
* Revision 1.5  2003/04/02 17:49:18  thiessen
* allow pdb id's in structure import dialog
*
* Revision 1.4  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.3  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.2  2002/02/27 16:29:41  thiessen
* add model type flag to general mime type
*
* Revision 1.1  2001/10/30 02:54:12  thiessen
* add Biostruc cache
*
*/
