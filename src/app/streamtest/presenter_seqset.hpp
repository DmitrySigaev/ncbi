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
* Author:
*
* File Description:
*
*
* Command lines: -batch -mode=get-title -i e:/ncbi/testing/gbpri1.aso -rf -ri 1000
*                -i e:/ncbi/testing/manyflysequences.sqn -rf -ri 100
* ===========================================================================
*/

#ifndef __presenter_seqset_hpp__
#define __presenter_seqset_hpp__

//  ============================================================================
class CSeqSetPresenter
//  ============================================================================
    : public CSeqEntryPresenter
{
public:
    //  ------------------------------------------------------------------------
    CSeqSetPresenter()
    //  ------------------------------------------------------------------------
        : m_repititions( 0 )
    {
    };

    //  ------------------------------------------------------------------------
    virtual void Initialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryPresenter::Initialize( args );  
        m_is.reset( 
            CObjectIStream::Open(
                (args["binary"] ? eSerial_AsnBinary : eSerial_AsnText), 
                args["i"].AsInputFile() ) );
        m_repititions = args["count"].AsInteger();
    };

    //  ------------------------------------------------------------------------
    virtual void Run(
        CSeqEntryProcess* process )
    //  ------------------------------------------------------------------------
    {   
        CSeqEntryPresenter::Run( process );

        CRef< CSeq_entry > se( new CSeq_entry );
        GetSeqEntry( se );
        if ( ! se ) {
            return;
        }
        for ( int i=0; i < m_repititions; ++i ) {
            Process( se );
        }

        if (m_report_final) {
            FinalReport();
        }
    };

    //  ------------------------------------------------------------------------
    virtual void Process(
        CRef< CSeq_entry > se )
    //  ------------------------------------------------------------------------
    {
        try {
            if ( m_process ) {
                m_process->SeqEntryInitialize( se );

                m_stopwatch.Restart();

                m_process->SeqEntryProcess();
                
                if ( m_stopwatch.IsRunning() ) {
                    double elapsed = m_stopwatch.Elapsed();
                    m_stopwatch.Stop();
                    m_total_time += elapsed;
                    m_diff_time += elapsed;
                }

            m_process->SeqEntryFinalize();
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
        if ( m_report_interval && 
            ! (m_process->GetObjectCount() % m_report_interval) ) 
        {
            ProgressReport();
        }
    };

protected:    
    //  ------------------------------------------------------------------------
    void GetSeqEntry( 
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        m_is->SetStreamPos( 0 );

        try {
            CRef<CSeq_submit> sub(new CSeq_submit());
            *m_is >> *sub;
            se.Reset( sub->SetData().SetEntrys().front() );
            return;
        }
        catch( ... ) {
            m_is->SetStreamPos( 0 );
        }
        try {
            *m_is >> *se;
            return;
        }
        catch( ... ) {
            m_is->SetStreamPos( 0 );
        }
        try {
		    CRef<CBioseq> bs( new CBioseq );
	        *m_is >> *bs;
            se->SetSeq( bs.GetObject() );
            return;
        }
        catch( ... ) {
            m_is->SetStreamPos( 0 );
        }
        try {
		    CRef<CBioseq_set> bss( new CBioseq_set );
	        *m_is >> *bss;
            se->SetSet( bss.GetObject() );
            return;
        }
        catch( ... ) {
//          m_is->SetStreamPos( 0 );
        }
    };

protected:       
    auto_ptr<CObjectIStream> m_is;
    int m_repititions;
};

#endif
