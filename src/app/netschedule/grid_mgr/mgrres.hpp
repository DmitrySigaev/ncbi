#ifndef NCBI_GRID_MGR_RES__HPP
#define NCBI_GRID_MGR_RES__HPP

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
* Author:  Maxim Didenko
*
*/

#include <cgi/ncbires.hpp>

BEGIN_NCBI_SCOPE 

class CGridMgrResource : public CNcbiResource    // see ncbires.hpp
{
public:

    CGridMgrResource( CNcbiRegistry& config );
    virtual ~CGridMgrResource();   
   
    // define the command to be executed when no other command matches
   
    virtual CNcbiCommand* GetDefaultCommand( void ) const;
    
};

END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2005/06/27 12:52:40  didenko
* Added grid manager cgi
*
* ===========================================================================
*/

#endif /* NCBI_GRID_MGR_RES__HPP */
