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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Definition of CWinMaskUStat class.
 *
 */

#ifndef C_WIN_MASK_USTAT_H
#define C_WIN_MASK_USTAT_H

#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

class CWinMaskUstat : public CObject
{
    public:

        class CWinMaskUstatException : public CException
        {
            public:

                enum EErrCode
                {
                    eBadState
                };

                virtual const char * GetErrCodeString() const;

                NCBI_EXCEPTION_DEFAULT( CWinMaskUstatException, CException );
        };

        explicit CWinMaskUstat( CNcbiOstream & os )
            : out_stream( os ), state( start )
        {}

        virtual ~CWinMaskUstat() {}

        void setUnitSize( Uint1 us );
        void setUnitCount( Uint4 unit, Uint4 count );
        void setComment( const string & msg ) { doSetComment( msg ); }
        void setParam( const string & name, Uint4 value );
        void setBlank() { doSetBlank(); }

    protected:

        virtual void doSetUnitSize( Uint4 us ) = 0;
        virtual void doSetUnitCount( Uint4 unit, Uint4 count ) = 0;
        virtual void doSetComment( const string & msg ) = 0;
        virtual void doSetParam( const string & name, Uint4 value ) = 0;
        virtual void doSetBlank() = 0;

        CNcbiOstream & out_stream;

    private:

        CWinMaskUstat( const CWinMaskUstat & );
        CWinMaskUstat( CWinMaskUstat & );

        enum 
        {
            start,
            ulen,
            udata,
            thres
        } state;
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * ========================================================================
 */
