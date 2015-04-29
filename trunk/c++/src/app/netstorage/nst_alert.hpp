#ifndef NETSTORAGE_ALERT__HPP
#define NETSTORAGE_ALERT__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   Net storage server alerts
 *
 */


#include "nst_precise_time.hpp"
#include <corelib/ncbimtx.hpp>
#include <connect/services/json_over_uttp.hpp>

#include <map>


BEGIN_NCBI_SCOPE


enum EAlertType {
    eUnknown = -1,
    eStartupConfig = 0,
    eReconfigure = 1,
    ePidFile = 2,
    eDB = 3,
    eAccess = 4,
    eConfigOutOfSync = 5,
    eDecryptDBPass = 6,
    eDecryptAdminNames = 7,
    eDBConnect = 8,
    eDecryptInNetStorageAPI = 9
};

enum EAlertAckResult {
    eNotFound = 0,
    eAlreadyAcknowledged = 1,
    eAcknowledged = 2
};


struct SNSTAlertAttributes
{
    CNSTPreciseTime     m_LastDetectedTimestamp;
    CNSTPreciseTime     m_AcknowledgedTimestamp;
    bool                m_On;
    size_t              m_Count;            // total, i.e. since startup
    size_t              m_CountSinceAck;    // since acknowledge
    string              m_User;
    string              m_Message;

    SNSTAlertAttributes() :
        m_LastDetectedTimestamp(CNSTPreciseTime::Current()),
        m_AcknowledgedTimestamp(), m_On(true), m_Count(1),
        m_CountSinceAck(1)
    {}

    CJsonNode Serialize(void) const;
};




class CNSTAlerts
{
    public:
        void Register(enum EAlertType alert_type,
                      const string &  message);
        enum EAlertAckResult Acknowledge(const string &  alert_id,
                                         const string &  user);
        enum EAlertAckResult Acknowledge(enum EAlertType alert_type,
                                         const string &  user);
        CJsonNode Serialize(void) const;

    private:
        mutable CFastMutex          m_Lock;
        map< enum EAlertType,
             SNSTAlertAttributes >  m_Alerts;

        enum EAlertType x_IdToType(const string &  alert_id) const;
        string x_TypeToId(enum EAlertType  type) const;
};



END_NCBI_SCOPE

#endif /* NETSTORAGE_ALERT__HPP */

