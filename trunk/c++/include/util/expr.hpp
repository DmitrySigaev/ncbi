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
 * Author: Sergey Sikorskiy
 *
 * File Description: 
 *      Expresiion parsing and evaluation.
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

#if defined(NCBI_OS_MSWIN)
#define INT_FORMAT "I64"
#elif SIZEOF_LONG == 8
#define INT_FORMAT "l"
#else 
#define INT_FORMAT "ll"
#endif


#define BINARY(opd) (opd >= ePOW)


////////////////////////////////////////////////////////////////////////////////
class CExprSymbol;

class NCBI_XUTIL_EXPORT CExprValue 
{ 
public:
    CExprValue(void);
    template <typename VT> CExprValue(VT value);
    CExprValue(const CExprValue& value);

public:
    enum EValue { 
        eINT, 
        eFLOAT,
        eBOOL
    };

public:
    EValue GetType(void) const
    {
        return m_Tag;
    }
    void SetType(EValue type)
    {
        m_Tag = type;
    }

    double GetDouble(void) const
    { 
        switch (m_Tag) {
            case eINT:
                return static_cast<double>(ival);
            case eBOOL:
                return bval ? 1.0 : 0.0;
            default:
                break;
        }

        return fval; 
    }

    Int8 GetInt(void) const
    { 
        switch (m_Tag) {
            case eFLOAT:
                return static_cast<Int8>(fval);
            case eBOOL:
                return bval ? 1 : 0;
            default:
                break;
        }

        return ival; 
    }

    bool GetBool(void) const
    { 
        switch (m_Tag) {
            case eINT:
                return ival != 0;
            case eFLOAT:
                return fval != 0.0;
            default:
                break;
        }

        return bval; 
    }

public:
    union { 
        Int8    ival;
        double  fval;
        bool    bval;
    };

    CExprSymbol*    m_Var;
    int             m_Pos;

private:
    EValue          m_Tag;
};

template <> CExprValue::CExprValue(Int8 value);
template <> CExprValue::CExprValue(double value);
template <> CExprValue::CExprValue(bool value);

////////////////////////////////////////////////////////////////////////////////
class NCBI_XUTIL_EXPORT CExprSymbol 
{ 
public:
    enum ESymbol { 
        eVARIABLE, 
        eIFUNC1, 
        eIFUNC2,
        eFFUNC1, 
        eFFUNC2,
        eBFUNC1, 
        eBFUNC2 
    };

public:
    CExprSymbol(void);
    template <typename VT> CExprSymbol(const char* name, VT value);
    ~CExprSymbol(void);

public:
    ESymbol         m_Tag;
    void*           m_Funct;
    CExprValue      m_Val;
    string          m_Name;
    CExprSymbol*    m_Next;
};
	       
template <> CExprSymbol::CExprSymbol(const char* name, Int8 value);
template <> CExprSymbol::CExprSymbol(const char* name, double value);
template <> CExprSymbol::CExprSymbol(const char* name, bool value);
template <> CExprSymbol::CExprSymbol(const char* name, Int8 (*value)(Int8));
template <> CExprSymbol::CExprSymbol(const char* name, Int8 (*value)(Int8, Int8));
template <> CExprSymbol::CExprSymbol(const char* name, double (*value)(double));
template <> CExprSymbol::CExprSymbol(const char* name, double (*value)(double, double));
template <> CExprSymbol::CExprSymbol(const char* name, bool (*value)(bool));
template <> CExprSymbol::CExprSymbol(const char* name, bool (*value)(bool, bool));


////////////////////////////////////////////////////////////////////////////////
class NCBI_XUTIL_EXPORT CExprParser
{
public:
    CExprParser(void);
    ~CExprParser(void);

public:
    void Parse(const char* str);

    const CExprValue& GetResult(void) const
    {
        if (m_v_sp != 1) {
            ReportError("Result is not available");
        }

        return m_VStack[0];
    }
    
    template <typename VT> 
    CExprSymbol* AddSymbol(const char* name, VT value);

private:
    enum EOperator { 
        eBEGIN, eOPERAND, eERROR, eEND, 
        eLPAR, eRPAR, eFUNC, ePOSTINC, ePOSTDEC,
        ePREINC, ePREDEC, ePLUS, eMINUS, eNOT, eCOM,
        ePOW,
        eMUL, eDIV, eMOD,
        eADD, eSUB, 
        eASL, eASR, eLSR, 
        eGT, eGE, eLT, eLE,     
        eEQ, eNE, 
        eAND,
        eXOR,
        eOR,
        eSET, eSETADD, eSETSUB, eSETMUL, eSETDIV, eSETMOD, eSETASL, eSETASR, eSETLSR, 
        eSETAND, eSETXOR, eSETOR, eSETPOW,
        eCOMMA,
        eTERMINALS
    };

    typedef Int8    (*TIFunc1)(Int8);
    typedef Int8    (*TIFunc2)(Int8,Int8);
    typedef double  (*TFFunc1)(double);
    typedef double  (*TFFunc2)(double, double);
    typedef bool    (*TBFunc1)(bool);
    typedef bool    (*TBFunc2)(bool, bool);

private:
    EOperator Scan(bool operand);
    bool Assign(void);

    static void ReportError(int pos, char* msg) {printf("pos: %u, msg: %s\n", pos, msg);}
    void ReportError(char* msg) const { ReportError(m_Pos-1, msg); }

    EOperator IfChar(
            char c, EOperator val, 
            EOperator val_def);
    EOperator IfElseChar(
            char c1, EOperator val1, 
            char c2, EOperator val2, 
            EOperator val_def);
    EOperator IfLongest2ElseChar(
            char c1, char c2, 
            EOperator val_true_longest, 
            EOperator val_true, 
            EOperator val_false, 
            EOperator val_def);

private:
    enum {hash_table_size = 1013};
    CExprSymbol* hash_table[hash_table_size];

    enum {max_stack_size = 256};
    enum {max_expression_length = 1024};

    static int sm_lpr[eTERMINALS];
    static int sm_rpr[eTERMINALS];

    CExprValue  m_VStack[max_stack_size];
    int         m_v_sp;
    EOperator   m_OStack[max_stack_size];
    int         m_o_sp;
    const char* m_Buf;
    int         m_Pos;
    int         m_TmpVarCount;
};


////////////////////////////////////////////////////////////////////////////////
// Inline methods.
//

NCBI_XUTIL_EXPORT
unsigned string_hash_function(const char* p);

template <typename VT> 
inline
CExprSymbol* CExprParser::AddSymbol(const char* name, VT value)
{
    unsigned h = string_hash_function(name) % hash_table_size;
    CExprSymbol* sp = NULL;

    for (sp = hash_table[h]; sp != NULL; sp = sp->m_Next) { 
        if (sp->m_Name.compare(name) == 0) { 
            return sp;
        }
    }

    sp = new CExprSymbol(name, value);

    sp->m_Next = hash_table[h];
    hash_table[h] = sp;

    return sp;
}

inline
CExprParser::EOperator 
CExprParser::IfChar(
        char c, EOperator val, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c) { 
        m_Pos += 1;
        return val;
    }

    return val_def;
}

inline
CExprParser::EOperator 
CExprParser::IfElseChar(
        char c1, EOperator val1, 
        char c2, EOperator val2, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c1) { 
        m_Pos += 1;
        return val1;
    } else if (m_Buf[m_Pos] == c2) { 
        m_Pos += 1;
        return val2;
    }

    return val_def;
}

inline
CExprParser::EOperator 
CExprParser::IfLongest2ElseChar(
        char c1, char c2, 
        EOperator val_true_longest, 
        EOperator val_true, 
        EOperator val_false, 
        EOperator val_def)
{
    if (m_Buf[m_Pos] == c1) { 
        m_Pos += 1;
        return IfChar(c2, val_true_longest, val_true);
    } else if (m_Buf[m_Pos] == c2) { 
        m_Pos += 1;
        return val_false;
    }

    return val_def;
}

END_NCBI_SCOPE


