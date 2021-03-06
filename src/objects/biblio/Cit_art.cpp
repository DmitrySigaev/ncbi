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
 *   using specifications from the ASN data definition file
 *   'biblio.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/general/Date.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CCit_art::~CCit_art(void)
{
}


bool CCit_art::GetLabelV1(string* label, TLabelFlags flags) const
{
    const CCit_jour*  journal = 0;
    const CCit_book*  book = 0;
    const CImprint*   imprint = 0;
    const CAuth_list* authors = 0;
    const CTitle*     title = 0;
    const string*     titleunique = 0;
    if ( IsSetAuthors() ) {
        authors = &GetAuthors();
    }
    if ( IsSetTitle() ) {
        titleunique = &GetTitle().GetTitle();
    }
    switch ( GetFrom().Which() ) {
    case C_From::e_Journal:
        journal = &GetFrom().GetJournal();
        imprint = &journal->GetImp();
        title = &journal->GetTitle();
        break;
    case C_From::e_Book:
        book = &GetFrom().GetBook();
        imprint = &book->GetImp();
        if (!authors) {
            authors = &book->GetAuthors();
        }
        break;
    case C_From::e_Proc:
        book = &GetFrom().GetProc().GetBook();
        imprint = &book->GetImp();
        if (!authors) {
            authors = &book->GetAuthors();
        }
    default:
        break;
    }
    return x_GetLabelV1(label, (flags & fLabel_Unique) != 0, authors, imprint,
                        title, book, journal, 0, 0, titleunique);
}   


// Based on FormatCit(Book)Art from the C Toolkit's api/asn2gnb5.c.
bool CCit_art::GetLabelV2(string* label, TLabelFlags flags) const
{
    switch (GetFrom().Which()) {
    case C_From::e_not_set:
        return false;
    case C_From::e_Journal:
        return GetFrom().GetJournal()
            .GetLabel(label, flags | fLabel_ISO_JTA, eLabel_V2);
    case C_From::e_Book:
        return x_GetLabelV2(label, flags, GetFrom().GetBook());
    case C_From::e_Proc:
        return x_GetLabelV2(label, flags, GetFrom().GetProc().GetBook());
    }

    return false;
}


bool CCit_art::x_GetLabelV2(string* label, TLabelFlags flags,
                            const CCit_book& book)
{
    const CImprint& imp    = book.GetImp();
    int             prepub = imp.CanGetPrepub() ? imp.GetPrepub() : 0;
    string          year   = GetParenthesizedYear(imp.GetDate());

    MaybeAddSpace(label);

    if (prepub == CImprint::ePrepub_submitted
        ||  prepub == CImprint::ePrepub_other) {
        *label += "Unpublished " + year;
        return true;
    }

    string title = book.GetTitle().GetTitle();
    if (title.size() < 3) {
        *label += '.';
        return false;
    }

    *label += "(in) ";
    if (book.GetAuthors().GetLabel(label, flags, eLabel_V2)) {
        size_t n = book.GetAuthors().GetNameCount();
        if (n > 1) {
            *label += " (Eds.);";
        } else if (n == 1) {
            *label += " (Ed.);";
        }
        *label += '\n';
    }

    *label += NStr::ToUpper(title);

    const string* volume = imp.CanGetVolume() ? &imp.GetVolume() : NULL;
    if (HasText(volume)  &&  *volume != "0") {
        *label += ", Vol. " + *volume;
        if ((flags & fLabel_FlatNCBI) != 0) {
            NoteSup(label, imp);
        }
    }
    if (imp.CanGetPages()) {
        string pages = FixPages(imp.GetPages());
        if (HasText(pages)) {
            *label += ": " + pages;
        }
    }
    *label += ";\n";

    if (imp.CanGetPub()
        &&  imp.GetPub().GetLabel(label, flags, eLabel_V1)) { // sic
        // "V1" taken over by MakeAffilStr translation
        *label += ' ';
    }
    *label += year;

    if ((flags & fLabel_FlatNCBI) != 0
        &&  prepub == CImprint::ePrepub_in_press) {
        *label += " In press";
    }

    return true;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1880, CRC32: 30f0bfed */
