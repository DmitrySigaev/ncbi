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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   ASN.1/XML RPC client generator
*
* ===========================================================================
*/

#include <serial/datatool/rpcgen.hpp>

#include <serial/datatool/choicetype.hpp>
#include <serial/datatool/classstr.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/generate.hpp>
#include <serial/datatool/srcutil.hpp>
#include <serial/datatool/stdstr.hpp>

BEGIN_NCBI_SCOPE


// Does all the actual work
class CClientPseudoTypeStrings : public CClassTypeStrings
{
public:
    CClientPseudoTypeStrings(const CClientPseudoDataType& source);

    void GenerateClassCode(CClassCode& code, CNcbiOstream& getters,
                           const string& methodPrefix, bool haveUserClass,
                           const string& classPrefix) const;

private:
    const CClientPseudoDataType& m_Source;
};


void s_SplitName(const string& name, string& type, string& field)
{
    for (SIZE_TYPE pos = name.find('.');  pos != NPOS;
         pos = name.find('.', pos + 1)) {
        if (islower(name[pos + 1])) {
            type.assign(name, 0, pos);
            field.assign(name, pos + 1, NPOS);
            return;
        }
    }
    type.assign(name);
    field.erase();
}


const CChoiceDataType* s_ChoiceType(const CDataType* dtype,
                                    const string& element)
{
    const CDataType* dtype2 = dtype;
    if ( !element.empty() ) {
        const CDataContainerType* dct
            = dynamic_cast<const CDataContainerType*>(dtype);
        if ( !dct ) {
            NCBI_THROW(CException, eUnknown,
                       dtype->GlobalName() + " is not a container type");
        }
        bool found = false;
        iterate (CDataContainerType::TMembers, it, dct->GetMembers()) {
            if ((*it)->GetName() == element) {
                found = true;
                dtype2 = (*it)->GetType()->Resolve();
                break;
            }
        }
        if (!found) {
            NCBI_THROW(CException, eUnknown,
                       dtype->GlobalName() + " has no element " + element);
        }
    }
    const CChoiceDataType* choicetype
        = dynamic_cast<const CChoiceDataType*>(dtype2);
    if ( !choicetype ) {
        NCBI_THROW(CException, eUnknown,
                   dtype2->GlobalName() + " is not a choice type");
    }
    return choicetype;
}


CClientPseudoDataType::CClientPseudoDataType(const CCodeGenerator& generator,
                                             const string& class_name)
    : m_Generator(generator), m_ClassName(class_name)
{
    // Just take the first potential module; should normally give sane
    // results.
    SetParent(generator.GetMainModules().GetModuleSets().front()
              ->GetModules().front().get(),
              class_name);

    s_SplitName(generator.GetConfig().Get("client", "request"),
                m_RequestType, m_RequestElement);
    s_SplitName(generator.GetConfig().Get("client", "reply"),
                m_ReplyType, m_ReplyElement);
    if (m_RequestType.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "No request type supplied for " + m_ClassName);
    } else if (m_ReplyType.empty()) {
        NCBI_THROW(CException, eUnknown,
                   "No reply type supplied for " + m_ClassName);
    }

    m_RequestDataType = m_Generator.ResolveMain(m_RequestType);
    m_ReplyDataType = m_Generator.ResolveMain(m_ReplyType);
    _ASSERT(m_RequestDataType  &&  m_ReplyDataType);
    m_RequestChoiceType = s_ChoiceType(m_RequestDataType, m_RequestElement);
    m_ReplyChoiceType   = s_ChoiceType(m_ReplyDataType,   m_ReplyElement);
}


AutoPtr<CTypeStrings>
CClientPseudoDataType::GenerateCode(void) const
{
    return new CClientPseudoTypeStrings(*this);
}


CClientPseudoTypeStrings::CClientPseudoTypeStrings
(const CClientPseudoDataType& source)
    : CClassTypeStrings(kEmptyStr, source.m_ClassName), m_Source(source)
{
    // SetClassNamespace(generator.GetNamespace()); // not defined(!)
    SetParentClass("CRPCClient<" + source.m_RequestDataType->ClassName()
                   + ", " + source.m_ReplyDataType->ClassName() + '>',
                   CNamespace::KNCBINamespace, "serial/rpcbase");
    SetObject(true);
    SetHaveUserClass(true);
    SetHaveTypeInfo(false);
}


void CClientPseudoTypeStrings::GenerateClassCode(CClassCode& code,
                                                 CNcbiOstream& /* getters */,
                                                 const string& /* methodPfx */,
                                                 bool /* haveUserClass */,
                                                 const string& /* classPfx */)
    const
{
    const string&         class_name = m_Source.m_ClassName;
    string                class_base = class_name + "_Base";
    const CCodeGenerator& generator  = m_Source.m_Generator;
    string                treq       = class_base + "::TRequest";
    string                trep       = class_base + "::TReply";

    // Pull in the relevant headers, and add corresponding typedefs
    code.HPPIncludes().insert(m_Source.m_RequestDataType->FileName());
    code.ClassPublic() << "    typedef "
                       << m_Source.m_RequestDataType->ClassName()
                       << " TRequest;\n";
    code.HPPIncludes().insert(m_Source.m_ReplyDataType->FileName());
    code.ClassPublic() << "    typedef "
                       << m_Source.m_ReplyDataType->ClassName()
                       << " TReply;\n";
    if ( !m_Source.m_RequestElement.empty() ) {
        code.HPPIncludes().insert(m_Source.m_RequestChoiceType->FileName());
        code.ClassPublic() << "    typedef "
                           << m_Source.m_RequestChoiceType->ClassName()
                           << " TRequestChoice;\n";
        code.ClassPrivate() << "    TRequest m_DefaultRequest;\n\n";
    } else {
        code.ClassPublic() << "    typedef TRequest TRequestChoice;\n";
    }
    {{
        string element;
        if ( !m_Source.m_ReplyElement.empty() ) {
            code.HPPIncludes().insert(m_Source.m_ReplyChoiceType->FileName());
            code.ClassPublic() << "    typedef "
                               << m_Source.m_ReplyChoiceType->ClassName()
                               << " TReplyChoice;\n";
            element = ".Set" + Identifier(m_Source.m_ReplyElement, true) + "()";
        } else {
            code.ClassPublic() << "    typedef TReply TReplyChoice;\n";
        }
        code.ClassPrivate()
            << "    TReplyChoice& x_Choice(TReply& reply);\n";
        code.MethodStart(true)
            << trep << "Choice& " << class_base << "::x_Choice(" << trep
            << "& reply)\n"
            << "{\n    return reply" << element << ";\n}\n\n";
    }}

    {{
        // Figure out arguments to parent's constructor
        string service = generator.GetConfig().Get("client", "service");
        string format  = generator.GetConfig().Get("client", "serialformat");
        string args;
        if (service.empty()) {
            ERR_POST(Warning << "No service name provided for " << class_name);
            args = "kEmptyStr";
        } else {
            args = '\"' + NStr::PrintableString(service) + '\"';
        }
        if ( !format.empty() ) {
            args += ", eSerial_" + format;
        }
        code.AddInitializer("Tparent", args);
    }}

    // Add appropriate infrastructure if TRequest is not itself the choice
    // (m_DefaultRequest declared earlier to reduce ugliness)
    if ( !m_Source.m_RequestElement.empty() ) {
        code.ClassPublic()
            << "\n"
            << "    virtual const TRequest& GetDefaultRequest(void) const;\n"
            << "    virtual TRequest&       SetDefaultRequest(void);\n"
            << "    virtual void            SetDefaultRequest(const TRequest& request);\n"
            << "\n"
            // This should just be a simple using-declaration, but that breaks
            // on GCC 2.9x at least, even with a full parent class name :-/
            // << "    using Tparent::Ask;\n"
            << "    virtual void Ask(const TRequest& req, TReply& reply);\n"
            << "    virtual void Ask(const TRequestChoice& req, TReply& reply);\n\n";

        // inline methods
        code.MethodStart(true)
            << "const " << treq << "& " << class_base
            << "::GetDefaultRequest(void) const\n"
            << "{\n    return m_DefaultRequest;\n}\n\n";
        code.MethodStart(true)
            << treq << "& " << class_base << "::SetDefaultRequest(void)\n"
            << "{\n    return m_DefaultRequest;\n}\n\n";
        code.MethodStart(true)
            << "void " << class_base << "::SetDefaultRequest(const " << treq
            << "& request)\n"
            << "{\n    m_DefaultRequest.Assign(request);\n}\n\n\n";
        // See comment above about using-declarations vs. GCC 2.9x. :-/
        code.MethodStart(true)
            << "void " << class_base << "::Ask(const " << treq << "& request, "
            << trep << "& reply)\n"
            << "{\n    Tparent::Ask(request, reply);\n}\n\n\n";

        code.MethodStart(false)
            << "void " << class_base << "::Ask(const " << treq
            << "Choice& req, " << class_base << "::TReply& reply)\n"
            << "{\n"
            << "    TRequest request;\n"
            << "    request.Assign(m_DefaultRequest);\n"
            // We have to copy req because SetXxx() wants a non-const ref.
            << "    request.Set" << Identifier(m_Source.m_RequestElement, true)
            << "().Assign(req);\n"
            << "    Ask(request, reply);\n"
            << "}\n\n\n";
    }

    // Scan choice types for interesting elements
    typedef CChoiceDataType::TMembers       TChoices;
    typedef map<string, const CDataMember*> TChoiceMap;
    const TChoices& choices   = m_Source.m_RequestChoiceType->GetMembers();
    TChoiceMap      reply_map;
    bool            has_init  = false, has_fini = false, has_error = false;
    iterate (TChoices, it, choices) {
        const string& name = (*it)->GetName();
        if (name == "init") {
            CNcbiOstrstream oss;
            (*it)->GetType()->PrintASN(oss, 0);
            string type = CNcbiOstrstreamToString(oss);
            if (type == "NULL") {
                has_init = true;
            } else {
                ERR_POST(Warning << "Ignoring non-null init (type " << type
                         << ") in " << m_Source.m_RequestChoiceType);
            }
        } else if (name == "fini") {
            CNcbiOstrstream oss;
            (*it)->GetType()->PrintASN(oss, 0);
            string type = CNcbiOstrstreamToString(oss);
            if (type == "NULL") {
                has_fini = true;
            } else {
                ERR_POST(Warning << "Ignoring non-null init (type " << type
                         << ") in " << m_Source.m_RequestChoiceType);
            }
        }
    }
    iterate (TChoices, it, m_Source.m_ReplyChoiceType->GetMembers()) {
        const string& name = (*it)->GetName();
        reply_map[name] = it->get();
        if (name == "error") {
            has_error = true;
        }
    }

    if (has_init) {
        code.ClassProtected()
            << "    void x_Connect(void);\n";
        code.MethodStart(false)
            << "void " << class_base << "::x_Connect(void)\n"
            << "{\n"
            << "    Tparent::x_Connect();\n"
            << "    AskInit(true);\n"
            << "}\n\n";
    }
    if (has_fini) {
        code.ClassProtected()
            << "    void x_Disconnect(void);\n";
        code.MethodStart(false)
            << "void " << class_base << "::x_Disconnect(void)\n"
            << "{\n"
            << "    AskFini(true);\n" // ignore/downgrade errors?
            << "    Tparent::x_Disconnect();\n"
            << "}\n\n";
    }

    // Make sure the reply's choice is correct
    code.ClassPrivate()
        << "    TReplyChoice& x_CheckReply(TReply& reply, TReplyChoice::E_Choice wanted);\n";
    code.MethodStart(true) // inline due to widespread use
        << trep << "Choice& " << class_base << "::x_CheckReply(" << trep
        << "& reply, " << trep << "Choice::E_Choice wanted)\n"
        << "{\n"
        << "    TReplyChoice& rc = x_Choice(reply);\n"
        << "    if (rc.Which() == wanted) {\n"
        << "        return rc; // ok\n";
    if (has_error) {
        code.Methods(true)
            << "    } else if (rc.IsError()) {\n"
            << "        CNcbiOstrstream oss;\n"
            << "        oss << \"" << class_name
            << ": server error: \" << rc.GetError();\n"
            << "        NCBI_THROW(CException, eUnknown, CNcbiOstrstreamToString(oss));\n";
    }
    code.Methods(true)
        << "    } else {\n"
        << "        rc.ThrowInvalidSelection(wanted);\n"
        << "    }\n"
        << "    return rc; // avoid spurious warnings about returning without a value\n"
        << "}\n\n";

    // Finally, generate all the actual Ask* methods....
    iterate (TChoices, it, choices) {
        typedef AutoPtr<CTypeStrings> TTypeStr;
        string name  = (*it)->GetName();
        string reply = m_Source.m_Generator.GetConfig().Get("client",
                                                            "reply." + name);
        if (reply.empty()) {
            reply = name;
        } else if (reply == "special") {
            continue;
        }
        TChoiceMap::const_iterator rm = reply_map.find(reply);
        if (rm == reply_map.end()) {
            NCBI_THROW(CException, eUnknown,
                       "Invalid reply type " + reply + " for " + name);
        }

        string           method   = "Ask" + Identifier(name);
        const CDataType* req_type = (*it)->GetType()->Resolve();
        string           req_class;
        if ( !req_type->GetParentType() ) {
            req_class = req_type->ClassName();
        } else {
            TTypeStr typestr = req_type->GetFullCType();
            typestr->GeneratePointerTypeCode(code);
            req_class = typestr->GetCType(code.GetNamespace());
        }

        const CDataType* rep_type = rm->second->GetType()->Resolve();
        string           rep_class;
        bool             use_cref = false;
        if ( !rep_type->GetParentType() ) {
            rep_class = "CRef<" + rep_type->ClassName() + '>';
            use_cref  = true;
            code.CPPIncludes().insert(rep_type->FileName());
        } else {
            TTypeStr typestr = rep_type->GetFullCType();
            typestr->GeneratePointerTypeCode(code);
            rep_class = typestr->GetCType(code.GetNamespace());
        }
        code.ClassPublic()
            << "    virtual " << rep_class << ' ' << method << "\n"
            << "        (const " << req_class
            << "& req, TReply* reply = 0);\n\n";
        code.MethodStart(false)
            << rep_class << ' ' << class_base << "::" << method
            << "(const " << req_class << "& req, " << trep << "* reply)\n"
            << "{\n"
            << "    TRequestChoice request;\n"
            << "    TReply         reply0;\n"
            << "    request.Set" << Identifier(name) << "(req);\n"
            << "    if ( !reply ) {\n"
            << "        reply = &reply0;\n"
            << "    }\n"
            << "    Ask(request, *reply);\n";
        if (use_cref) {
            code.Methods(false)
                << "    return " << rep_class
                << "(&x_CheckReply(*reply, TReplyChoice::e_"
                << Identifier(reply) << ").Set" << Identifier(reply)
                << "());\n"
                << "}\n\n";
        } else {
            code.Methods(false)
                << "    return x_CheckReply(*reply, TReplyChoice::e_"
                << Identifier(reply) << ").Get" << Identifier(reply) << "();\n"
                << "}\n\n";
        }
    }
}


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/11/13 19:55:11  ucko
* Distinguish between named and anonymous types rather than between
* heterogeneous containers and everything else -- fixes handling of aliases.
*
* Revision 1.1  2002/11/13 00:46:08  ucko
* Add RPC client generator; CVS logs to end in generate.?pp
*
*
* ===========================================================================
*/
