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
 * Author:  Sergey Sikorskiy
 *
 * File Description:  Windows DLL function hooking
 *
 */

#include <ncbi_pch.hpp>

#if defined(NCBI_OS_MSWIN)

    #include "ncbi_win_hook.hpp"
    #include <algorithm>

BEGIN_NCBI_SCOPE

namespace NWinHook
{

    ///////////////////////////////////////////////////////////////////////////////
    const char* 
    CWinHookException::GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eDbghelp:  return "eDbghelp";
        default:        return CException::GetErrCodeString();
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    /// class CPEi386
    ///
    class CPEi386 {
    private:
        CPEi386(void);
        ~CPEi386(void) throw();

    public:
        static CPEi386& GetInstance(void);

        PVOID GetIAT(HMODULE base, int section) const;

    private:
        typedef PVOID (WINAPI *FImageDirectoryEntryToData) (
            PVOID Base,
            BOOLEAN MappedAsImage,
            USHORT DirectoryEntry,
            PULONG Size
            );

        HMODULE m_ModDbghelp;
        FImageDirectoryEntryToData m_ImageDirectoryEntryToData;

        friend class CSafeStaticPtr<CPEi386>;
    };

    /////////////////////////////////////////////////////////////////////////////
    static BOOL IsToolHelpSupported(void)
    {
        BOOL    bResult(FALSE);
        HMODULE hModToolHelp;
        PROC    pfnCreateToolhelp32Snapshot;

        hModToolHelp = ::LoadLibrary( "KERNEL32.DLL" );
        if (hModToolHelp != NULL) {
            pfnCreateToolhelp32Snapshot = 
            ::GetProcAddress(hModToolHelp,
                             "CreateToolhelp32Snapshot"
                             );
            bResult = (pfnCreateToolhelp32Snapshot != NULL);
            ::FreeLibrary(hModToolHelp);
        }

        return (bResult);
    }


    static BOOL IsPsapiSupported(void)
    {
        BOOL bResult = FALSE;
        HMODULE hModPSAPI = NULL;

        hModPSAPI = ::LoadLibrary( "PSAPI.DLL" );
        bResult = (hModPSAPI != NULL);
        if (bResult) {
            ::FreeLibrary(hModPSAPI);
        }

        return (bResult);
    }

    static HMODULE ModuleFromAddress(PVOID pv)
    {
        MEMORY_BASIC_INFORMATION mbi;

        return ((::VirtualQuery(pv, &mbi, sizeof(mbi)) != 0)
               ? (HMODULE) mbi.AllocationBase : NULL);
    }


    ///////////////////////////////////////////////////////////////////////////////
    typedef struct {
        char szCalleeModName[MAX_PATH];
        char szFuncName[MAX_PATH];
    } API_FUNC_ID;

    const API_FUNC_ID MANDATORY_API_FUNCS[] =
    {
        {"Kernel32.dll", "LoadLibraryA"},
        {"Kernel32.dll", "LoadLibraryW"},
        {"Kernel32.dll", "LoadLibraryExA"},
        {"Kernel32.dll", "LoadLibraryExW"},
        {"Kernel32.dll", "GetProcAddress"}
    };

    // This macro evaluates to the number of elements in MANDATORY_API_FUNCS
#define NUMBER_OF_MANDATORY_API_FUNCS (sizeof(MANDATORY_API_FUNCS) / \
    sizeof(MANDATORY_API_FUNCS[0]))

    ///////////////////////////////////////////////////////////////////////////////
    CLibHandler::CLibHandler(void)
    {
    }

    CLibHandler::~CLibHandler(void)
    {
    }

    ///////////////////////////////////////////////////////////////////////////////
    CModuleInstance::CModuleInstance(char *pszName, HMODULE hModule):
    m_pszName(NULL),
    m_hModule(NULL)
    {
        SetName(pszName);
        SetModule(hModule);
    }

    CModuleInstance::~CModuleInstance(void)
    {
        ReleaseModules();

        if (NULL != m_pszName) {
            delete [] m_pszName;
        }
    }


    void CModuleInstance::AddModule(CModuleInstance* pModuleInstance)
    {
        m_pInternalList.push_back(pModuleInstance);
    }

    void CModuleInstance::ReleaseModules(void)
    {
        ITERATE(TInternalList, it, m_pInternalList) {
            delete *it;
        }
        m_pInternalList.clear();
    }

    char* CModuleInstance::GetName(void) const 
    {
        return (m_pszName);
    }

    char* CModuleInstance::GetBaseName(void) const 
    {
        char *pdest;
        int  ch = '\\';
        // Search backward
        pdest = strrchr(m_pszName, ch);
        if (pdest != NULL) {
            return (&pdest[1]);
        }
        else {
            return (m_pszName);
        }
    }

    void CModuleInstance::SetName(char *pszName)
    {
        if (NULL != m_pszName) {
            delete [] m_pszName;
        }
        if ((NULL != pszName) && (strlen(pszName))) {
            m_pszName = new char[strlen(pszName) + 1];
            strcpy(m_pszName, pszName);
        }
        else {
            m_pszName = new char[strlen("\0") + 1];
            strcpy(m_pszName, "\0");
        }

    }

    HMODULE CModuleInstance::GetModule() const 
    {
        return (m_hModule);
    }

    void CModuleInstance::SetModule(HMODULE module)
    {
        m_hModule = module;
    }


    ///////////////////////////////////////////////////////////////////////////////
    CExeModuleInstance::CExeModuleInstance(CLibHandler* pLibHandler,
                                           char*        pszName,
                                           HMODULE      hModule,
                                           DWORD        dwProcessId
                                           ):
    CModuleInstance(pszName, hModule),
    m_pLibHandler(pLibHandler),
    m_dwProcessId(dwProcessId)
    {

    }

    CExeModuleInstance::~CExeModuleInstance(void)
    {

    }

    DWORD CExeModuleInstance::GetProcessId(void) const
    {
        return (m_dwProcessId);
    }

    BOOL CExeModuleInstance::PopulateModules(void)
    {
        _ASSERT(m_pLibHandler);
        return (m_pLibHandler->PopulateModules(this));
    }


    size_t CExeModuleInstance::GetModuleCount(void) const
    {
        return (m_pInternalList.size());
    }

    CModuleInstance* CExeModuleInstance::GetModuleByIndex(DWORD dwIndex) const
    {
        if (m_pInternalList.size() <= dwIndex) {
            return (NULL);
        }

        return (m_pInternalList[dwIndex]);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CPsapiHandler::CPsapiHandler(void):
    m_hModPSAPI(NULL),
    m_pfnEnumProcesses(NULL),
    m_pfnEnumProcessModules(NULL),
    m_pfnGetModuleFileNameExA(NULL)
    {

    }

    CPsapiHandler::~CPsapiHandler(void)
    {
        Finalize();
    }

    BOOL CPsapiHandler::Initialize(void)
    {
        BOOL bResult = FALSE;
        //
        // Get to the 3 functions in PSAPI.DLL dynamically.  We can't
        // be sure that PSAPI.DLL has been installed
        //
        if (NULL == m_hModPSAPI) {
            m_hModPSAPI = ::LoadLibraryA("PSAPI.DLL");
        }

        if (NULL != m_hModPSAPI) {
            m_pfnEnumProcesses = 
            (FEnumProcesses)
            ::GetProcAddress(m_hModPSAPI,"EnumProcesses");

            m_pfnEnumProcessModules = 
            (FEnumProcessModules)
            ::GetProcAddress(m_hModPSAPI, "EnumProcessModules");

            m_pfnGetModuleFileNameExA = 
            (FGetModuleFileNameExA)
            ::GetProcAddress(m_hModPSAPI, "GetModuleFileNameExA");

        }

        bResult =
        m_pfnEnumProcesses
        &&  m_pfnEnumProcessModules
        &&  m_pfnGetModuleFileNameExA;

        return (bResult);
    }

    void CPsapiHandler::Finalize(void)
    {
        if (NULL != m_hModPSAPI) {
            ::FreeLibrary(m_hModPSAPI);
        }
    }

    BOOL CPsapiHandler::PopulateModules(CModuleInstance* pProcess)
    {
        BOOL   bResult = TRUE;
        CModuleInstance  *pDllModuleInstance = NULL;

        if (Initialize() == TRUE) {
            DWORD pidArray[1024];
            DWORD cbNeeded;
            DWORD nProcesses;
            // EnumProcesses returns an array with process IDs
            if (m_pfnEnumProcesses(pidArray, sizeof(pidArray), &cbNeeded)) {
                // Determine number of processes
                nProcesses = cbNeeded / sizeof(DWORD);
                // Release the container
                pProcess->ReleaseModules();

                for (DWORD i = 0; i < nProcesses; i++) {
                    HMODULE hModuleArray[1024];
                    HANDLE  hProcess;
                    DWORD   pid = pidArray[i];
                    DWORD   nModules;
                    // Let's open the process
                    hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | 
                                                PROCESS_VM_READ,
                                             FALSE, pid);

                    if (!hProcess) {
                        continue;
                    }

                    if (static_cast<CExeModuleInstance*>(pProcess)->GetProcessId() != pid) {
                        ::CloseHandle(hProcess);
                        continue;
                    }

                    // EnumProcessModules function retrieves a handle for
                    // each module in the specified process.
                    if (!m_pfnEnumProcessModules(hProcess, 
                                                 hModuleArray,
                                                 sizeof(hModuleArray), 
                                                 &cbNeeded)) {
                        ::CloseHandle(hProcess);
                        continue;
                    }

                    // Calculate number of modules in the process
                    nModules = cbNeeded / sizeof(hModuleArray[0]);

                    for (DWORD j = 0; j < nModules; j++) {
                        HMODULE hModule = hModuleArray[j];
                        char    szModuleName[MAX_PATH];

                        m_pfnGetModuleFileNameExA(hProcess,
                                                  hModule,
                                                  szModuleName,
                                                  sizeof(szModuleName)
                                                  );

                        if (0 == j) {   // First module is the EXE.
                            // Do nothing.
                        }
                        else {    // Not the first module.  It's a DLL
                            pDllModuleInstance = 
                                new CModuleInstance(szModuleName,
                                                    hModule
                                                    );
                            pProcess->AddModule(pDllModuleInstance);
                        }
                    }
                    ::CloseHandle(hProcess);    // We're done with this process handle
                }
                bResult = TRUE;
            }
            else {
                bResult = FALSE;
            }
        }
        else {
            bResult = FALSE;
        }
        return (bResult);
    }


    BOOL CPsapiHandler::PopulateProcess(DWORD dwProcessId, 
                                        BOOL bPopulateModules)
    {
        BOOL   bResult = TRUE;
        CExeModuleInstance* pProcessInfo;

        if (TRUE == Initialize()) {
            HMODULE hModuleArray[1024];
            HANDLE  hProcess;
            DWORD   nModules;
            DWORD   cbNeeded;
            hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | 
                                        PROCESS_VM_READ,
                                     FALSE,
                                     dwProcessId
                                     );
            if (hProcess) {
                if (!m_pfnEnumProcessModules(hProcess,
                                             hModuleArray,
                                             sizeof(hModuleArray),
                                             &cbNeeded
                                             )) {
                    ::CloseHandle(hProcess);
                }
                else {
                    // Calculate number of modules in the process
                    nModules = cbNeeded / sizeof(hModuleArray[0]);

                    for (DWORD j = 0; j < nModules; j++) {
                        HMODULE hModule = hModuleArray[j];
                        char    szModuleName[MAX_PATH];

                        m_pfnGetModuleFileNameExA(hProcess,
                                                  hModule,
                                                  szModuleName,
                                                  sizeof(szModuleName)
                                                  );

                        if (0 == j) {   // First module is the EXE.  Just add it to the map
                            pProcessInfo = new CExeModuleInstance(this,
                                                                  szModuleName,
                                                                  hModule,
                                                                  dwProcessId
                                                                  );
                            m_pProcess.reset(pProcessInfo);

                            if (bPopulateModules) {
                                pProcessInfo->PopulateModules();
                            }

                            break;
                        }
                    }
                    ::CloseHandle(hProcess);
                }
            }
        }
        else {
            bResult = FALSE;
        }
        return (bResult);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CToolhelpHandler::CToolhelpHandler(void)
    {
    }

    CToolhelpHandler::~CToolhelpHandler(void)
    {
    }


    BOOL CToolhelpHandler::Initialize(void)
    {
        BOOL           bResult = FALSE;
        HINSTANCE      hInstLib;

        hInstLib = ::LoadLibraryA("Kernel32.DLL");
        if (NULL != hInstLib) {
            // We must link to these functions of Kernel32.DLL explicitly. Otherwise
            // a module using this code would fail to load under Windows NT, which does not
            // have the Toolhelp32 functions in the Kernel32.
            m_pfnCreateToolhelp32Snapshot = 
                (FCreateToolHelp32Snapshot)
                    ::GetProcAddress(hInstLib, "CreateToolhelp32Snapshot");
            m_pfnProcess32First = (FProcess32First)
                                  ::GetProcAddress(hInstLib, "Process32First");
            m_pfnProcess32Next = (FProcess32Next)
                                 ::GetProcAddress(hInstLib, "Process32Next");
            m_pfnModule32First = (FModule32First)
                                 ::GetProcAddress(hInstLib, "Module32First");
            m_pfnModule32Next = (FModule32Next)
                                ::GetProcAddress(hInstLib, "Module32Next");

            ::FreeLibrary( hInstLib );

            bResult = m_pfnCreateToolhelp32Snapshot &&
                      m_pfnProcess32First &&
                      m_pfnProcess32Next &&
                      m_pfnModule32First &&
                      m_pfnModule32Next;
        }

        return (bResult);
    }

    BOOL CToolhelpHandler::PopulateModules(CModuleInstance* pProcess)
    {
        BOOL   bResult = TRUE;
        CModuleInstance  *pDllModuleInstance = NULL;
        HANDLE hSnapshot = INVALID_HANDLE_VALUE;

        hSnapshot = m_pfnCreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE,
            static_cast<CExeModuleInstance*>(pProcess)->GetProcessId());

        MODULEENTRY32 me = { sizeof(me)};

        for (BOOL bOk = ModuleFirst(hSnapshot, &me); bOk; bOk = ModuleNext(hSnapshot, &me)) {
            // We don't need to add to the list the process itself.
            // The module list should keep references to DLLs only
            if (0 != stricmp(pProcess->GetBaseName(), me.szModule)) {
                pDllModuleInstance = new CModuleInstance(me.szExePath, me.hModule);
                pProcess->AddModule(pDllModuleInstance);
            }
            else {
                // However, we should fix up the module of the EXE, because
                // th32ModuleID member has meaning only to the tool help functions
                // and it is not usable by Win32 API elements.
                pProcess->SetModule( me.hModule );
            }
        }

        if (hSnapshot != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hSnapshot);
        }

        return (bResult);
    }

    BOOL CToolhelpHandler::ModuleFirst(HANDLE hSnapshot, PMODULEENTRY32 pme) const
    {
        return (m_pfnModule32First(hSnapshot, pme));
    }

    BOOL CToolhelpHandler::ModuleNext(HANDLE hSnapshot, PMODULEENTRY32 pme) const
    {
        return (m_pfnModule32Next(hSnapshot, pme));
    }

    BOOL CToolhelpHandler::ProcessFirst(HANDLE hSnapshot, PROCESSENTRY32* pe32) const
    {
        return (m_pfnProcess32First(hSnapshot, pe32));
    }

    BOOL CToolhelpHandler::ProcessNext(HANDLE hSnapshot, PROCESSENTRY32* pe32) const
    {
        return (m_pfnProcess32Next(hSnapshot, pe32));
    }

    BOOL CToolhelpHandler::PopulateProcess(DWORD dwProcessId, BOOL bPopulateModules)
    {
        BOOL   bResult    = FALSE;
        CExeModuleInstance* pProcessInfo;
        HANDLE hSnapshot  = INVALID_HANDLE_VALUE;

        if (Initialize() == TRUE) {
            hSnapshot = m_pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, dwProcessId);

            PROCESSENTRY32 pe32 = { sizeof(pe32)};

            for (BOOL bOk = ProcessFirst(hSnapshot, &pe32); 
                bOk; 
                bOk = ProcessNext(hSnapshot, &pe32)) {
                if ((dwProcessId != NULL) && (dwProcessId != pe32.th32ProcessID)) {
                    continue;
                }

                pProcessInfo = 
                new CExeModuleInstance(this,
                                       pe32.szExeFile,
                                       NULL,
                                       pe32.th32ProcessID
                                       );
                m_pProcess.reset(pProcessInfo);
                if (bPopulateModules) {
                    pProcessInfo->PopulateModules();
                }

                if (dwProcessId != NULL) {
                    break;
                }
            }

            if (hSnapshot != INVALID_HANDLE_VALUE) {
                ::CloseHandle(hSnapshot);
            }

            bResult = TRUE;
        }

        return (bResult);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CTaskManager::CTaskManager():
    m_pLibHandler(NULL)
    {
        if (IsPsapiSupported()) {
            m_pLibHandler = new CPsapiHandler;
        }
        else {
            if (IsToolHelpSupported())
                m_pLibHandler = new CToolhelpHandler;
        }
    }

    CTaskManager::~CTaskManager(void)
    {
        delete m_pLibHandler;
    }

    BOOL CTaskManager::PopulateProcess(DWORD dwProcessId, 
                                       BOOL bPopulateModules) const
    {
        _ASSERT(m_pLibHandler);
        return (m_pLibHandler->PopulateProcess(dwProcessId, bPopulateModules));
    }


    CExeModuleInstance* CTaskManager::GetProcess(void) const
    {
        _ASSERT(m_pLibHandler);
        return (m_pLibHandler->GetExeModuleInstance());
    }



    void
    CHookedFunctions::UnHookAllFuncs(void)
    {
        CHookedFunction* pHook;

        ITERATE(TFunctionList, it, m_FunctionList) {
            pHook = it->second;
            pHook->UnHookImport();
            delete pHook;
        }
        m_FunctionList.clear();
    }

    ///////////////////////////////////////////////////////////////////////////////
    //
    // The highest private memory address (used for Windows 9x only)
    //
    PVOID CHookedFunction::sm_pvMaxAppAddr = NULL;
    //
    // The PUSH opcode on x86 platforms
    //
    const BYTE cPushOpCode = 0x68;

    CHookedFunction::CHookedFunction(PCSTR pszCalleeModName,
                                     PCSTR pszFuncName,
                                     PROC  pfnOrig,
                                     PROC  pfnHook
                                     ) :
    m_bHooked(FALSE),
    m_pfnOrig(pfnOrig),
    m_pfnHook(pfnHook) 
    {
        strcpy(m_szCalleeModName, pszCalleeModName);
        strcpy(m_szFuncName, pszFuncName);

        if (sm_pvMaxAppAddr == NULL) {
            // Functions with address above lpMaximumApplicationAddress require
            // special processing (Windows 9x only)
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            sm_pvMaxAppAddr = si.lpMaximumApplicationAddress;
        }

        if (m_pfnOrig > sm_pvMaxAppAddr) {
            // The address is in a shared DLL; the address needs fixing up
            PBYTE pb = (PBYTE) m_pfnOrig;
            if (pb[0] == cPushOpCode) {
                // Skip over the PUSH op code and grab the real address
                PVOID pv = * (PVOID*) &pb[1];
                m_pfnOrig = (PROC) pv;
            }
        }
    }


    CHookedFunction::~CHookedFunction(void)
    {
        UnHookImport();
    }

    PCSTR CHookedFunction::GetCalleeModName(void) const
    {
        return (const_cast<PCSTR>(m_szCalleeModName));
    }

    PCSTR CHookedFunction::GetFuncName(void) const
    {
        return (const_cast<PCSTR>(m_szFuncName));
    }

    PROC CHookedFunction::GetPfnHook(void) const
    {
        return (m_pfnHook);
    }

    PROC CHookedFunction::GetPfnOrig(void) const
    {
        return (m_pfnOrig);
    }

    BOOL CHookedFunction::HookImport(void)
    {
        m_bHooked = DoHook(TRUE, m_pfnOrig, m_pfnHook);

        return (m_bHooked);
    }

    BOOL CHookedFunction::UnHookImport(void)
    {
        if (m_bHooked) {
            m_bHooked = !DoHook(FALSE, m_pfnHook, m_pfnOrig);
        }

        return (!m_bHooked);
    }

    BOOL CHookedFunction::ReplaceInAllModules(BOOL  bHookOrRestore,
                                              PCSTR pszCalleeModName,
                                              PROC  pfnCurrent,
                                              PROC  pfnNew
                                              )
    {
        BOOL bResult = FALSE;

        if ((NULL != pfnCurrent) && (NULL != pfnNew)) {
            BOOL                bReplace  = FALSE;
            CExeModuleInstance  *pProcess = NULL;
            CTaskManager        taskManager;
            CModuleInstance     *pModule;

            // Retrieves information about current process and modules.
            // The taskManager dynamically decides whether to use ToolHelp
            // library or PSAPI
            taskManager.PopulateProcess(::GetCurrentProcessId(), TRUE);
            pProcess = taskManager.GetProcess();
            if (NULL != pProcess) {
                // Enumerates all modules loaded by (pProcess) process
                for (size_t i = 0; i < pProcess->GetModuleCount(); ++i) {
                    pModule = pProcess->GetModuleByIndex(i);
                    bReplace = (pModule->GetModule() != 
                        ModuleFromAddress(CApiHookMgr::GetProcAddressWindows));

                    // We don't hook functions in our own modules
                    if (bReplace)
                        // Hook this function in this module
                        bResult = ReplaceInOneModule(pszCalleeModName,
                                                     pfnCurrent,
                                                     pfnNew,
                                                     pModule->GetModule()
                                                     ) || bResult;

                }
                // Hook this function in the executable as well
                bResult = ReplaceInOneModule(pszCalleeModName,
                                             pfnCurrent,
                                             pfnNew,
                                             pProcess->GetModule()
                                             ) || bResult;
            }
        }
        return (bResult);
    }


    BOOL CHookedFunction::ReplaceInOneModule(PCSTR   pszCalleeModName,
                                             PROC    pfnCurrent,
                                             PROC    pfnNew,
                                             HMODULE hmodCaller
                                             )
    {
        BOOL bResult = FALSE;
        __try {

            // Get the address of the module's import section
            PIMAGE_IMPORT_DESCRIPTOR pImportDesc =
                static_cast<PIMAGE_IMPORT_DESCRIPTOR>
                    (CPEi386::GetInstance().GetIAT(hmodCaller, 
                                                   IMAGE_DIRECTORY_ENTRY_IMPORT));

            // Does this module has import section ?
            if (pImportDesc == NULL) {
                __leave;
            }

            // Loop through all descriptors and
            // find the import descriptor containing references to callee's functions
            while (pImportDesc->Name) {
                PSTR pszModName = (PSTR)((PBYTE) hmodCaller + pImportDesc->Name);
                if (stricmp(pszModName, pszCalleeModName) == 0) {
                    break;   // Found
                }
                pImportDesc++;
            }

            // Does this module import any functions from this callee ?
            if (pImportDesc->Name == 0) {
                __leave;
            }

            // Get caller's IAT
            PIMAGE_THUNK_DATA pThunk =
            (PIMAGE_THUNK_DATA)( (PBYTE) hmodCaller + pImportDesc->FirstThunk );

            // Replace current function address with new one
            while (pThunk->u1.Function) {
                // Get the address of the function address
                PROC* ppfn = (PROC*) &pThunk->u1.Function;
                // Is this the function we're looking for?
                BOOL bFound = (*ppfn == pfnCurrent);
                // Is this Windows 9x
                if (!bFound && (*ppfn > sm_pvMaxAppAddr)) {
                    PBYTE pbInFunc = (PBYTE) *ppfn;

                    // Is this a wrapper (debug thunk) represented by PUSH instruction?
                    if (pbInFunc[0] == cPushOpCode) {
                        ppfn = (PROC*) &pbInFunc[1];
                        // Is this the function we're looking for?
                        bFound = (*ppfn == pfnCurrent);
                    }
                }

                if (bFound) {
                    MEMORY_BASIC_INFORMATION mbi;
                    ::VirtualQuery(ppfn, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                    // In order to provide writable access to this part of the
                    // memory we need to change the memory protection
                    if (FALSE == ::VirtualProtect(mbi.BaseAddress,
                                                  mbi.RegionSize,
                                                  // Use copy-on-write protection
                                                  // PAGE_WRITECOPY, 
                                                  PAGE_READWRITE,
                                                  &mbi.Protect)
                       ) {
                        __leave;
                    }

                    // Hook the function.
                    *ppfn = *pfnNew;
                    bResult = TRUE;
                    // Restore the protection back
                    DWORD dwOldProtect;
                    ::VirtualProtect(mbi.BaseAddress,
                                     mbi.RegionSize,
                                     mbi.Protect,
                                     &dwOldProtect
                                     );
                    break;
                }

                pThunk++;
            }
        }
        __finally {
            // do nothing
        }
        // This function is not in the caller's import section
        return (bResult);
    }

    BOOL CHookedFunction::DoHook(BOOL bHookOrRestore,
                                 PROC pfnCurrent,
                                 PROC pfnNew
                                 )
    {
        // Hook this function in all currently loaded modules
        return (ReplaceInAllModules(bHookOrRestore,
                                    m_szCalleeModName,
                                    pfnCurrent,
                                    pfnNew
                                    ));
    }

    // Indicates whether the hooked function is mandatory one
    BOOL CHookedFunction::IsMandatory(void)
    {
        BOOL bResult = FALSE;
        API_FUNC_ID apiFuncId;
        for (int i = 0; i < NUMBER_OF_MANDATORY_API_FUNCS; ++i) {
            apiFuncId = MANDATORY_API_FUNCS[i];
            if ((0 == stricmp(apiFuncId.szCalleeModName, m_szCalleeModName)) &&
                (0 == stricmp(apiFuncId.szFuncName, m_szFuncName))) {
                bResult = TRUE;
                break;
            }
        }

        return (bResult);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CHookedFunctions::CHookedFunctions(void)
    {
    }

    CHookedFunctions::~CHookedFunctions(void)
    {
    }

    ///////////////////////////////////////////////////////////////////////////////
    static BOOL ExtractModuleFileName(char* pszFullFileName)
    {
        BOOL  bResult = FALSE;

        if (TRUE != ::IsBadReadPtr(pszFullFileName, MAX_PATH)) {
            char  *pdest;
            int   ch = '\\';

            // Search backward
            pdest = strrchr(pszFullFileName, ch);
            if (pdest != NULL)
                strcpy(pszFullFileName, &pdest[1]);

            bResult = TRUE;
        }

        return (bResult);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CHookedFunction* CHookedFunctions::GetHookedFunction(HMODULE hmodOriginal,
                                                         PCSTR   pszFuncName
                                                         )
    {
        char szFileName[MAX_PATH];
        ::GetModuleFileName(hmodOriginal, szFileName, MAX_PATH);
        // We must extract only the name and the extension
        ExtractModuleFileName(szFileName);

        return (GetHookedFunction(szFileName, pszFuncName));
    }

    BOOL CHookedFunctions::GetFunctionNameFromExportSection(
        HMODULE hmodOriginal,
        DWORD   dwFuncOrdinalNum,
        PSTR    pszFuncName
        )
    {
        BOOL bResult = FALSE;
        // Make sure we return a valid string (atleast an empty one)
        strcpy(pszFuncName, "\0");
        __try
        {
            // Get the address of the module's export section
            PIMAGE_EXPORT_DIRECTORY pExportDir =
                static_cast<PIMAGE_EXPORT_DIRECTORY>
                    (CPEi386::GetInstance().GetIAT(hmodOriginal, 
                                                   IMAGE_DIRECTORY_ENTRY_EXPORT));

            // Does this module has export section ?
            if (pExportDir == NULL) {
                __leave;
            }

            // Get the name of the DLL
            PSTR pszDllName = reinterpret_cast<PSTR>( pExportDir->Name + (DWORD)hmodOriginal);
            // Get the starting ordinal value. By default is 1, but
            // is not required to be so
            DWORD dwFuncNumber = pExportDir->Base;
            // The number of entries in the EAT
            size_t dwNumberOfExported = pExportDir->NumberOfFunctions;
            // Get the address of the ENT
            PDWORD pdwFunctions = (PDWORD)( pExportDir->AddressOfFunctions + (DWORD)hmodOriginal);
            //  Get the export ordinal table
            PWORD pwOrdinals = (PWORD)(pExportDir->AddressOfNameOrdinals + (DWORD)hmodOriginal);
            // Get the address of the array with all names
            DWORD *pszFuncNames =   (DWORD *)(pExportDir->AddressOfNames + (DWORD)hmodOriginal);

            PSTR pszExpFunName;

            // Walk through all of the entries and try to locate the
            // one we are looking for
            for (size_t i = 0; i < dwNumberOfExported; ++i, ++pdwFunctions) {
                DWORD entryPointRVA = *pdwFunctions;
                if (entryPointRVA == 0) {
                    // Skip over gaps in exported function
                    // ordinals (the entrypoint is 0 for
                    // these functions).
                    continue;               
                }
                                            
                // See if this function has an associated name exported for it.
                for (unsigned j=0; j < pExportDir->NumberOfNames; j++) {
                    // Note that pwOrdinals[x] return values starting form 0.. (not from 1)
                    if (pwOrdinals[j] == i) {
                        pszExpFunName = (PSTR)(pszFuncNames[j] + (DWORD)hmodOriginal);
                        // Is this the same ordinal value ?
                        // Notice that we need to add 1 to pwOrdinals[j] to get actual
                        // number
                        if (dwFuncOrdinalNum == pwOrdinals[j] + 1) {
                            if ((pszExpFunName != NULL) && (strlen(pszExpFunName) > 0)) {
                                strcpy(pszFuncName, pszExpFunName);
                            }
                            __leave;
                        }
                    }
                }
            }
        }
        __finally
        {
            // do nothing
        }
        // This function is not in the caller's import section
        return (bResult);
    }

    void CHookedFunctions::GetFunctionNameByOrdinal(PCSTR   pszCalleeModName,
                                                    DWORD   dwFuncOrdinalNum,
                                                    PSTR    pszFuncName
                                                    )
    {
        HMODULE hmodOriginal = ::GetModuleHandle(pszCalleeModName);
        // Take the name from the export section of the DLL
        GetFunctionNameFromExportSection(hmodOriginal, dwFuncOrdinalNum, pszFuncName);
    }


    CHookedFunction* CHookedFunctions::GetHookedFunction(PCSTR   pszCalleeModName,
                                                         PCSTR   pszFuncName
                                                         )
    {
        CHookedFunction* pHook = NULL;
        char szFuncName[MAX_PATH];
        // Prevent accessing invalid pointers and examine values
        // for APIs exported by ordinal
        if ((pszFuncName) &&
            ((DWORD)pszFuncName > 0xFFFF) &&
            strlen(pszFuncName)) {
            strcpy(szFuncName, pszFuncName);
        }
        else {
            GetFunctionNameByOrdinal(pszCalleeModName, (DWORD)pszFuncName, szFuncName);
        }
        // Search in the map only if we have found the name of the requested function
        if (strlen(szFuncName) > 0) {
            char szKey[MAX_PATH];
            // iterators can be used to check if an entry is in the map
            TFunctionList::const_iterator citr = m_FunctionList.find( szKey );
            if (citr != m_FunctionList.end())
                pHook = citr->second;
        }

        return (pHook);
    }


    BOOL CHookedFunctions::AddHook(CHookedFunction* pHook)
    {
        BOOL bResult = FALSE;
        if (NULL != pHook) {
            char szKey[MAX_PATH];
            // Find where szKey is or should be
            TFunctionList::iterator lb = m_FunctionList.lower_bound(szKey);
            // Adds pair(pszKey, pObject) to the map
            m_FunctionList.insert( lb, TFunctionList::value_type(szKey, pHook) );
            bResult = TRUE;
        }
        return (bResult);
    }

    BOOL CHookedFunctions::RemoveHook(CHookedFunction* pHook)
    {
        BOOL bResult = FALSE;
        try {
            if (NULL != pHook) {
                char szKey[MAX_PATH];
                // Find where szKey is located
                TFunctionList::iterator itr = m_FunctionList.find(szKey);
                if (itr != m_FunctionList.end()) {
                    delete itr->second;
                    m_FunctionList.erase(itr);
                }
                bResult = TRUE;
            }
        }
        catch (...) {
            bResult = FALSE;
        }
        return (bResult);
    }

    ///////////////////////////////////////////////////////////////////////////////
    CApiHookMgr::CApiHookMgr() :
    m_bSystemFuncsHooked(FALSE)
    {
        HookSystemFuncs();
    }

    CApiHookMgr::~CApiHookMgr(void)
    {
        UnHookAllFuncs();
    }

    void
    CApiHookMgr::operator =(const CApiHookMgr&)
    {
    }

    CApiHookMgr&
    CApiHookMgr::GetInstance(void)
    {
        static CSafeStaticPtr<CApiHookMgr> instance;

        return (instance.Get());
    }

    BOOL CApiHookMgr::HookSystemFuncs(void)
    {
        BOOL bResult;

        if (m_bSystemFuncsHooked != TRUE) {
            bResult = HookImport("Kernel32.dll",
                                 "LoadLibraryA",
                                 (PROC) CApiHookMgr::MyLoadLibraryA
                                 );
            bResult = HookImport("Kernel32.dll",
                                 "LoadLibraryW",
                                 (PROC) CApiHookMgr::MyLoadLibraryW
                                 ) || bResult;
            bResult = HookImport("Kernel32.dll",
                                 "LoadLibraryExA",
                                 (PROC) CApiHookMgr::MyLoadLibraryExA
                                 ) || bResult;
            bResult = HookImport("Kernel32.dll",
                                 "LoadLibraryExW",
                                 (PROC) CApiHookMgr::MyLoadLibraryExW
                                 ) || bResult;
            bResult = HookImport("Kernel32.dll",
                                 "GetProcAddress",
                                 (PROC) CApiHookMgr::MyGetProcAddress
                                 ) || bResult;

            m_bSystemFuncsHooked = bResult;
        }

        return (m_bSystemFuncsHooked);
    }

    void CApiHookMgr::UnHookAllFuncs(void)
    {
        m_pHookedFunctions.UnHookAllFuncs();
        m_bSystemFuncsHooked = FALSE;
    }

    // Indicates whether there is hooked function
    bool CApiHookMgr::HaveHookedFunctions(void) const
    {
        return (m_pHookedFunctions.HaveHookedFunctions());
    }

    CHookedFunction* CApiHookMgr::GetHookedFunction(HMODULE hmod,
                                                    PCSTR   pszFuncName
                                                    )
    {
        CFastMutexGuard guard(m_Mutex);

        return (m_pHookedFunctions.GetHookedFunction(hmod, pszFuncName));
    }

    // Hook up an API function
    BOOL CApiHookMgr::HookImport(PCSTR pszCalleeModName,
                                 PCSTR pszFuncName,
                                 PROC  pfnHook
                                 )
    {
        CFastMutexGuard guard(m_Mutex);

        BOOL                  bResult = FALSE;
        PROC                  pfnOrig = NULL;

        if (!m_pHookedFunctions.GetHookedFunction(
            pszCalleeModName,
            pszFuncName
            )) {

            pfnOrig = GetProcAddressWindows(
                ::GetModuleHandleA(pszCalleeModName),
                pszFuncName
                );

            // It's possible that the requested module is not loaded yet
            // so lets try to load it.
            if (pfnOrig != NULL) {
                HMODULE hmod = ::LoadLibraryA(pszCalleeModName);
                if (NULL != hmod) {
                    pfnOrig = GetProcAddressWindows(
                        ::GetModuleHandleA(pszCalleeModName),
                        pszFuncName
                        );
                }
            }

            if (pfnOrig != NULL) {
                bResult = AddHook(pszCalleeModName,
                                  pszFuncName,
                                  pfnOrig,
                                  pfnHook
                                  );
            }
        }

        return (bResult);
    }

    // Restores original API function address in IAT
    BOOL CApiHookMgr::UnHookImport(PCSTR pszCalleeModName,
                                   PCSTR pszFuncName
                                   )
    {
        CFastMutexGuard guard(m_Mutex);

        BOOL bResult = TRUE;
        try {
            bResult = RemoveHook(pszCalleeModName, pszFuncName);
        }
        NCBI_CATCH_ALL( kEmptyStr )

        return (bResult);
    }

    // Add a hook to the internally supported container
    BOOL CApiHookMgr::AddHook(PCSTR pszCalleeModName,
                              PCSTR pszFuncName,
                              PROC  pfnOrig,
                              PROC  pfnHook
                              )
    {
        BOOL             bResult = FALSE;
        CHookedFunction* pHook   = NULL;

        if (!m_pHookedFunctions.GetHookedFunction(pszCalleeModName,
                                                  pszFuncName
                                                  )) {
            pHook = new CHookedFunction(pszCalleeModName,
                                        pszFuncName,
                                        pfnOrig,
                                        pfnHook
                                        );

            // We must create the hook and insert it in the container
            pHook->HookImport();
            bResult = m_pHookedFunctions.AddHook(pHook);
        }

        return (bResult);
    }

    // Remove a hook from the internally supported container
    BOOL CApiHookMgr::RemoveHook(PCSTR pszCalleeModName,
                                 PCSTR pszFuncName
                                 )
    {
        BOOL             bResult = FALSE;
        CHookedFunction *pHook   = NULL;

        pHook = m_pHookedFunctions.GetHookedFunction(pszCalleeModName,
                                                     pszFuncName
                                                     );
        if (NULL != pHook) {
            bResult = pHook->UnHookImport();
            if (bResult) {
                bResult = m_pHookedFunctions.RemoveHook( pHook );
                if (bResult) {
                    delete pHook;
                }
            }
        }

        return (bResult);
    }


    // Used when a DLL is newly loaded after hooking a function
    void WINAPI CApiHookMgr::HackModuleOnLoad(HMODULE hmod, DWORD dwFlags)
    {
        // If a new module is loaded, just hook it
        if ((hmod != NULL) && ((dwFlags & LOAD_LIBRARY_AS_DATAFILE) == 0)) {
            CFastMutexGuard guard(m_Mutex);

            CHookedFunction* pHook;
            ITERATE(CHookedFunctions::TFunctionList, 
                    it, 
                    m_pHookedFunctions.m_FunctionList) {

                pHook = it->second;
                pHook->ReplaceInOneModule(pHook->GetCalleeModName(),
                                          pHook->GetPfnOrig(),
                                          pHook->GetPfnHook(),
                                          hmod
                                          );
            }
        }
    }

    HMODULE WINAPI CApiHookMgr::MyLoadLibraryA(PCSTR pszModuleName)
    {
        HMODULE hmod = ::LoadLibraryA(pszModuleName);
        GetInstance().HackModuleOnLoad(hmod, 0);

        return (hmod);
    }

    HMODULE WINAPI CApiHookMgr::MyLoadLibraryW(PCWSTR pszModuleName)
    {
        HMODULE hmod = ::LoadLibraryW(pszModuleName);
        GetInstance().HackModuleOnLoad(hmod, 0);

        return (hmod);
    }

    HMODULE WINAPI CApiHookMgr::MyLoadLibraryExA(PCSTR  pszModuleName,
                                                 HANDLE hFile,
                                                 DWORD  dwFlags)
    {
        HMODULE hmod = ::LoadLibraryExA(pszModuleName, hFile, dwFlags);
        GetInstance().HackModuleOnLoad(hmod, 0);

        return (hmod);
    }

    HMODULE WINAPI CApiHookMgr::MyLoadLibraryExW(PCWSTR pszModuleName,
                                                 HANDLE hFile,
                                                 DWORD dwFlags)
    {
        HMODULE hmod = ::LoadLibraryExW(pszModuleName, hFile, dwFlags);
        GetInstance().HackModuleOnLoad(hmod, 0);

        return (hmod);
    }

    FARPROC WINAPI CApiHookMgr::MyGetProcAddress(HMODULE hmod, 
                                                 PCSTR pszProcName)
    {
        // Get the original address of the function
        FARPROC pfn = GetProcAddressWindows(hmod, pszProcName);

        // Attempt to locate if the function has been hijacked
        CHookedFunction* pFuncHook =
        GetInstance().GetHookedFunction(hmod,
                                        pszProcName
                                        );

        if (pFuncHook != NULL) {
            // The address to return matches an address we want to hook
            // Return the hook function address instead
            pfn = pFuncHook->GetPfnHook();
        }

        return (pfn);
    }

    FARPROC WINAPI CApiHookMgr::GetProcAddressWindows(HMODULE hmod, 
                                                      PCSTR pszProcName)
    {
        return (::GetProcAddress(hmod, pszProcName));
    }

    /////////////////////////////////////////////////////////////////////////////
    CPEi386::CPEi386(void) :
        m_ModDbghelp(NULL),
        m_ImageDirectoryEntryToData(NULL)
    {
        m_ModDbghelp = ::LoadLibrary( "DBGHELP.DLL" );

        if (m_ModDbghelp != NULL) {
            m_ImageDirectoryEntryToData = 
                reinterpret_cast<FImageDirectoryEntryToData>(
                    ::GetProcAddress(m_ModDbghelp,
                                    "ImageDirectoryEntryToData"
                                    ));
            if (!m_ImageDirectoryEntryToData ) {
                NCBI_THROW(CWinHookException, 
                           eDbghelp, 
                           "Dbghelp.dll does not have "
                           "ImageDirectoryEntryToData symbol");
            }
        } else {
            NCBI_THROW(CWinHookException, 
                       eDbghelp, 
                       "Dbghelp.dll not found");
        }
    }

    CPEi386::~CPEi386(void) throw()
    {
        try {
            if (m_ModDbghelp) {
                ::FreeLibrary(m_ModDbghelp);
            }
        }
        NCBI_CATCH_ALL( kEmptyStr )
    }

    CPEi386& CPEi386::GetInstance(void)
    {
        static CSafeStaticPtr<CPEi386> instance(NULL, 
            CSafeStaticLifeSpan::eLifeSpan_Longest);

        return (instance.Get());
    }

    PVOID CPEi386::GetIAT(HMODULE base, int section) const
    {
        ULONG ulSize(0);

        return m_ImageDirectoryEntryToData(base,
                                           TRUE,
                                           section,
                                           &ulSize
                                           );
    }

    /////////////////////////////////////////////////////////////////////////////
    COnExitProcess::COnExitProcess(void)
    {
        CApiHookMgr::GetInstance().HookImport("Kernel32.DLL", 
                                              "ExitProcess", 
                                              (PROC)COnExitProcess::ExitProcess);
    }

    COnExitProcess::~COnExitProcess(void)
    {
        ClearAll();

        CApiHookMgr::GetInstance().UnHookImport("Kernel32.DLL", 
                                                "ExitProcess");
    }

    COnExitProcess&
    COnExitProcess::Instance(void)
    {
        static CSafeStaticPtr<COnExitProcess> instance;

        return (instance.Get());
    }

    void 
    COnExitProcess::Add(TFunct funct)
    {
        CFastMutexGuard mg(m_Mutex);

        vector<TFunct>::iterator it = find(m_Registry.begin(), 
                                           m_Registry.end(), 
                                           funct);
        if (it == m_Registry.end()) {
            m_Registry.push_back(funct);
        }
    }

    void 
    COnExitProcess::Remove(TFunct funct)
    {
        CFastMutexGuard mg(m_Mutex);

        m_Registry.erase(find(m_Registry.begin(), 
                              m_Registry.end(), 
                              funct));
    }

    void 
    COnExitProcess::ClearAll(void)
    {
        if (!m_Registry.empty()) {
            CFastMutexGuard mg(m_Mutex);
            if (!m_Registry.empty()) {
                // Run all functions ...
                ITERATE(vector<TFunct>, it, m_Registry) {
                    (*it)();
                }
                m_Registry.clear();
            }
        }
    }

    void WINAPI COnExitProcess::ExitProcess(UINT uExitCode)
    {
        COnExitProcess::Instance().ClearAll();

        ::ExitProcess(uExitCode);
    }

}

END_NCBI_SCOPE

#endif

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/04/05 14:05:48  ssikorsk
 * Initial version
 *
 * ===========================================================================
 */
