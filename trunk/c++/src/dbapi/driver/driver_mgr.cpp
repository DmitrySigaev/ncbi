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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver manager
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <corelib/ncbidll.hpp>

BEGIN_NCBI_SCOPE


class C_xDriverMgr : public I_DriverMgr
{
public:
    C_xDriverMgr(unsigned int nof_drivers= 16) {
        m_NofRoom= nof_drivers? nof_drivers : 16;
        m_Drivers= new SDrivers[m_NofRoom];
        m_NofDrvs= 0;
    }

    FDBAPI_CreateContext GetDriver(const string& driver_name,
                                   string* err_msg= 0);

    virtual void RegisterDriver(const string&        driver_name,
                                FDBAPI_CreateContext driver_ctx_func);

    virtual ~C_xDriverMgr() {
        delete [] m_Drivers;
    }

protected:
    bool LoadDriverDll(const string& driver_name, string* err_msg);

private:
    typedef void            (*FDriverRegister) (I_DriverMgr& mgr);
    typedef FDriverRegister (*FDllEntryPoint)  (void);

    struct SDrivers {
        string               drv_name;
        FDBAPI_CreateContext drv_func;
    } *m_Drivers;

    unsigned int m_NofDrvs;
    unsigned int m_NofRoom;
    CFastMutex m_Mutex1;
    CFastMutex m_Mutex2;
};


void C_xDriverMgr::RegisterDriver(const string&        driver_name,
                                  FDBAPI_CreateContext driver_ctx_func)
{
    if(m_NofDrvs < m_NofRoom) {
        CFastMutexGuard mg(m_Mutex2);
        for(unsigned int i= m_NofDrvs; i--; ) {
            if(m_Drivers[i].drv_name == driver_name) {
                m_Drivers[i].drv_func= driver_ctx_func;
                return;
            }
        }
        m_Drivers[m_NofDrvs++].drv_func= driver_ctx_func;
        m_Drivers[m_NofDrvs-1].drv_name= driver_name;
    }
    else {
        throw CDB_ClientEx(eDB_Error, 101, "C_xDriverMgr::RegisterDriver",
                           "No space left for driver registration");
    }

}


FDBAPI_CreateContext C_xDriverMgr::GetDriver(const string& driver_name,
                                             string* err_msg)
{
    CFastMutexGuard mg(m_Mutex1);
    unsigned int i;

    for(i= m_NofDrvs; i--; ) {
        if(m_Drivers[i].drv_name == driver_name) {
            return m_Drivers[i].drv_func;
        }
    }

    if (!LoadDriverDll(driver_name, err_msg)) {
        return 0;
    }

    for(i= m_NofDrvs; i--; ) {
        if(m_Drivers[i].drv_name == driver_name) {
            return m_Drivers[i].drv_func;
        }
    }

    throw CDB_ClientEx(eDB_Error, 200, "C_DriverMgr::GetDriver",
                       "internal error");
}


bool C_xDriverMgr::LoadDriverDll(const string& driver_name, string* err_msg)
{
    try {
        CDll drv_dll("dbapi_driver_" + driver_name);

        FDllEntryPoint entry_point;
        if ( !drv_dll.GetEntryPoint_Func("DBAPI_E_" + driver_name,
                                         &entry_point) ) {
            drv_dll.Unload();
            return false;
        }
        FDriverRegister reg = entry_point();
        if(!reg) {
            throw CDB_ClientEx(eDB_Fatal, 300, "C_DriverMgr::LoadDriverDll",
                               "driver reports an unrecoverable error "
                               "(e.g. conflict in libraries)");
        }
        reg(*this);
        return true;
    }
    catch (exception& e) {
        if(err_msg) *err_msg= e.what();
        return false;
    }
}


static C_xDriverMgr* s_DrvMgr= 0;
static int           s_DrvCount= 0;
DEFINE_STATIC_FAST_MUTEX(s_DrvMutex);

C_DriverMgr::C_DriverMgr(unsigned int nof_drivers)
{
    CFastMutexGuard mg(s_DrvMutex); // lock the mutex
    if(!s_DrvMgr) { // There is no driver manager yet
        s_DrvMgr= new C_xDriverMgr(nof_drivers);
    }
    ++s_DrvCount;
}


C_DriverMgr::~C_DriverMgr()
{
    CFastMutexGuard mg(s_DrvMutex); // lock the mutex
    if(--s_DrvCount <= 0) { // this is a last one
        delete s_DrvMgr;
        s_DrvMgr= 0;
        s_DrvCount= 0;
    }
}


FDBAPI_CreateContext C_DriverMgr::GetDriver(const string& driver_name,
                                            string* err_msg)
{
    return s_DrvMgr->GetDriver(driver_name, err_msg);
}


void C_DriverMgr::RegisterDriver(const string&        driver_name,
                                 FDBAPI_CreateContext driver_ctx_func)
{
    s_DrvMgr->RegisterDriver(driver_name, driver_ctx_func);
}

I_DriverContext* C_DriverMgr::GetDriverContext(const string&       driver_name,
                                               string*             err_msg,
                                               const map<string,string>* attr)
{
    FDBAPI_CreateContext create_context_func= GetDriver(driver_name, err_msg);

    if(!create_context_func)
        return 0;

    return create_context_func(attr);

}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.15  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.14  2003/11/19 15:33:55  ucko
 * Adjust for CDll API change.
 *
 * Revision 1.13  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.12  2002/09/19 18:47:31  soussov
 * LoadDriverDll now allows driver to report an unrecoverable error through NULL return from entry_point function
 *
 * Revision 1.11  2002/04/23 16:46:17  soussov
 * GetDriverContext added
 *
 * Revision 1.10  2002/04/12 18:48:30  soussov
 * makes driver_mgr working properly in mt environment
 *
 * Revision 1.9  2002/04/09 22:18:15  vakatov
 * Moved code from the header
 *
 * Revision 1.8  2002/04/04 23:59:37  soussov
 * bug in RegisterDriver fixed
 *
 * Revision 1.7  2002/04/04 23:57:39  soussov
 * return of error message from dlopen added
 *
 * Revision 1.6  2002/02/14 00:59:42  vakatov
 * Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
 *
 * Revision 1.5  2002/01/23 21:29:48  soussov
 * replaces map with array
 *
 * Revision 1.4  2002/01/20 07:26:58  vakatov
 * Fool-proofed to compile func_ptr/void_ptr type casts on all compilers.
 * Added virtual destructor.
 * Temporarily get rid of the MT-safety features -- they need to be
 * implemented here more carefully (in the nearest future).
 *
 * Revision 1.3  2002/01/20 05:24:42  vakatov
 * identation
 *
 * Revision 1.2  2002/01/17 23:19:13  soussov
 * makes gcc happy
 *
 * Revision 1.1  2002/01/17 22:17:25  soussov
 * adds driver manager
 *
 * ===========================================================================
 */
