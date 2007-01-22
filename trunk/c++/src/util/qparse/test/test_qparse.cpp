/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *   Test/demo for query parser
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/qparse/query_parse.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/// User object for the query tree. Carries simple int value
///
/// @internal
///
class CTestQueryEvalValue : public IQueryParseUserObject
{
public:
    CTestQueryEvalValue() : m_Value(0) {}
    virtual void Reset() { m_Value = 0; }
    virtual string GetVisibleValue() const { return NStr::IntToString(m_Value); };
    
    int GetValue() const { return m_Value; }
    void SetValue(int v) { m_Value = v; }
    
    
private:
    int m_Value;
};


/// Integer calculator for static query evaluation
/// Demonstrates how to create an execution machine on parsing tree
///
/// @internal
///
class CTestQueryEvalFunc
{
public: 
    CTestQueryEvalFunc()
    {}

    
    ETreeTraverseCode 
    operator()(CTreeNode<CQueryParseNode>& tr, int delta) 
    {
        CQueryParseNode& qnode = tr.GetValue();
        if (delta == 0 || delta == 1) {
            // If node has children, we skip it and process on the way back
            if (!tr.IsLeaf()) {
                return eTreeTraverse;
            }
        }
        
        // establish a user object as a carrier of 
        // execution info (intermidiate and final results)
        
        CTestQueryEvalValue*   eval_res = 0;
        IQueryParseUserObject* uo = qnode.GetUserObject();
        if (uo) {
            eval_res = dynamic_cast<CTestQueryEvalValue*>(uo);
            _ASSERT(eval_res);
        }
        if (eval_res == 0) {
            eval_res = new CTestQueryEvalValue();
            qnode.SetUserObject(eval_res);
        }
        
        int i0, i1, res; 
        
        
        switch (qnode.GetType()) {
        
        // value evaluation
        //
        case CQueryParseNode::eIdentifier:
        case CQueryParseNode::eString:
            {
            const string& v = qnode.GetStrValue();
            res = NStr::StringToInt(v);
            }
            break;
        case CQueryParseNode::eIntConst:
            res = qnode.GetInt();            
            break;
        case CQueryParseNode::eFloatConst:
            res = (int)qnode.GetDouble();
            break;
        case CQueryParseNode::eBoolConst:
            res = qnode.GetBool();
            break;
            
        // operation interpretor
        //
        case CQueryParseNode::eNot:
            ERR_POST("NOT not implemented");
            eval_res->SetValue(0);
            break;
        case CQueryParseNode::eFieldSearch:
            break;
        case CQueryParseNode::eAnd:
            GetArg2(tr, i0, i1);
            res = (i0 && i1);
            break;
        case CQueryParseNode::eOr:
            GetArg2(tr, i0, i1);
            res = (i0 || i1);
            break;
        case CQueryParseNode::eSub:
            GetArg2(tr, i0, i1);
            res = ((i0 != 0 ? 1 : 0) - (i1 != 0 ? 1 : 0));
            break;
        case CQueryParseNode::eXor:
            GetArg2(tr, i0, i1);
            res = ((i0 != 0 ? 1 : 0) ^ (i1 != 0 ? 1 : 0));
            break;
        case CQueryParseNode::eRange:
            ERR_POST("Range not implemented");
            break;
        case CQueryParseNode::eEQ:
            GetArg2(tr, i0, i1);
            res = (i0 == i1);
            break;
        case CQueryParseNode::eGT:
            GetArg2(tr, i0, i1);
            res = (i0 > i1);
            break;
        case CQueryParseNode::eGE:
            GetArg2(tr, i0, i1);
            res = (i0 >= i1);
            break;
        case CQueryParseNode::eLT:
            GetArg2(tr, i0, i1);
            res = (i0 < i1);
            break;
        case CQueryParseNode::eLE:
            GetArg2(tr, i0, i1);
            res = (i0 <= i1);
            break;
            
        default:
            ERR_POST("Unknown node type");
            eval_res->SetValue(0);
            break;        
        } // switch
        
        if (qnode.IsNot()) {
            res = !res;
        }
        eval_res->SetValue(res);
    
        return eTreeTraverse;
    }

private:

    /// get two arguments for a binary operation
    ///
    void GetArg2(CTreeNode<CQueryParseNode>& tr,
                 int&                        arg1,
                 int&                        arg2)
    {
        arg1 = arg2 = 0;
        CTreeNode<CQueryParseNode>::TNodeList_CI it =
            tr.SubNodeBegin();
        CTreeNode<CQueryParseNode>::TNodeList_CI it_end =
            tr.SubNodeEnd();
        if (it == it_end) {
            ERR_POST("No arguments!");
        }
        int i = 0;       
        for (; it != it_end; ++it, ++i) {
            const CTreeNode<CQueryParseNode>* arg_tree = *it;
            const CQueryParseNode& arg_qnode = arg_tree->GetValue();

            const CTestQueryEvalValue*   eval_res;
            const IQueryParseUserObject* uo = arg_qnode.GetUserObject();
            if (uo) {
                eval_res = dynamic_cast<const CTestQueryEvalValue*>(uo);
            } else {
                break;
            }
            if (i == 0) {
                arg1 = eval_res->GetValue();
            } else {
                arg2 = eval_res->GetValue();
                break; // take only two args
            }
        } // for
    }
};






/// @internal
class CTestQParse : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestQParse::Init(void)
{

    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_qparse",
                       "test query parse");
    SetupArgDescriptions(d.release());
}

/// @internal
static
void TestExpression(const char* q, bool cs = false,
                                   bool relax = false)
{
    CQueryParseTree qtree;
    NcbiCout << "---------------------------------------------------" << endl;
    NcbiCout << "Query:" << "'" << q << "'" << endl << endl;    
    qtree.Parse(q, 
                cs ? CQueryParseTree::eCaseSensitiveUpper :
                     CQueryParseTree::eCaseInsensitive,
                relax ? CQueryParseTree::eSyntaxRelax : 
                     CQueryParseTree::eSyntaxCheck
                );
    qtree.Print(NcbiCout);
    NcbiCout << "---------------------------------------------------" << endl;
}

/// @internal
static
void ParseFile(const string& file_name)
{
    unsigned q_total = 0;
    unsigned q_failed = 0;
    
    ifstream ifs(file_name.c_str());
    if (ifs.fail()) {
        cerr << "Cannot open: " << file_name << endl;
        return;
    }
    string line;
    while (getline(ifs, line,'\n'))
    {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            continue;
        }
        
        try {
            ++q_total;
            TestExpression(line.c_str(), true, true);
        } catch (CQueryParseException & ex) {
            cerr << ex.GetMsg() << endl;
            ++q_failed;
            continue;
        }
        
    } // while
    
    cout << endl << "Total=" << q_total << " Failed=" << q_failed << endl;
}

/// Parse and evaluate
///
/// @internal
///
void StaticEvaluate(const char* q, int res)
{
    CQueryParseTree qtree;
    qtree.Parse(q);
    
    CQueryParseTree::TNode* top_node = qtree.GetQueryTree();
    if (top_node == 0) {
        NcbiCerr << "No tree for query: " << q << endl;
        assert(0);
        return;
    }
    
    NcbiCout << "---------------------------------------------------" << endl;
    NcbiCout << "Query:" << "'" << q << "'" << endl << endl;    

    // query tree evaluation
    //    
    CTestQueryEvalFunc visitor;
    TreeDepthFirstTraverse(*top_node, visitor);
    
    qtree.Print(NcbiCout);
    
    
    IQueryParseUserObject* uo = top_node->GetValue().GetUserObject();
    assert(uo);
    CTestQueryEvalValue*   eval_res = dynamic_cast<CTestQueryEvalValue*>(uo);    
    assert(eval_res);
    if (res != eval_res->GetValue()) {
        NcbiCout << "Unexpected result:" << eval_res->GetValue()
                 << " should be:" << res << endl;
        assert(0);
    }
    
}


int CTestQParse::Run(void)
{
    {{
    const char* queries[] = {    
        "\n\r\n(asdf != 2)",
        "    asdf != 2",
        " 1 AND 0 ",
        " 1 OR 0 ",
        " 1 XOR 0 ",
        " 1 SUB 1",
        " 1 MINUS 1",
        " 1 && 1",
        " 1 || 1",
        " 13 NOT 1",
        " NOT 1",
        " 1 < 2",
        " 1 <= 1",
        " 1 > 1",
        " 200>=1002",
        " 200==1002",
        " 200 <> 1002",
        " 200 != 1002",
        " 100 OR \nVitamin C ",
        "(1 and 1)or 0",
        "((1 and 1)or 0)((1 && 1) or 0)",
        "\"Vitamin C\" \"aspirin\"\"2\"and \"D\"",  
        "a AND P[2]",
        "\"Pseudarthrosis\"[MeSH] OR \"Pseudarthrosis\"[MeSH] AND systematic neurons[Text word]",
        "\"J Sci Food Agric\"[Journal:__jrid5260] 1974",
        "vitamin 1993:1995 [dp]",
         "\"2005/04/17 12.23\"[MHDA]:\"2005/05/10 10.56\"[MHDA]"
    };    
    int l = sizeof (queries) / sizeof(queries[0]);
    for (int i = 0; i < l; ++i) {
        TestExpression(queries[i]);
    } // for
    
    }}
    
    NcbiCout << endl << endl;
    NcbiCout << "Misspelled queries.";
    NcbiCout << endl << endl;
   
   
    {{
    const char* queries[] = {    
        "vitamin c[MeSH",
        "vitamin \"c",
        "( asdf qwerty",
        "(12",
        "a==10 || ( asdf",
        "AND asdf",
        "(a1 AND a2) OR (aaa",
        "(a1 AND a2) OR OR aaa",
        "(a1 AND a2) value <= OR AND XOR 23"
    };    
    int l = sizeof (queries) / sizeof(queries[0]);
    for (int i = 0; i < l; ++i) {
        TestExpression(queries[i], true, true);
    } // for
    
    }}
   
    NcbiCout << endl << endl;
    NcbiCout << "Expression evaluation.";
    NcbiCout << endl << endl;
   
    StaticEvaluate("1==1", 1);
    StaticEvaluate("1!=1", 0);
    StaticEvaluate("1 <= 1", 1);
    StaticEvaluate("10 >= 1", 1);
    StaticEvaluate("1 <= 2 && 1 == 1", 1);
    StaticEvaluate("1 > 2 || 1 != 1", 0);
   
   
    //ParseFile("/net/garret/export/home/dicuccio/work/text-mining/sample-queries/unique-queries.50000");
    
    return 0;
}


int main(int argc, const char* argv[])
{
    CTestQParse app; 
    return app.AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log: test_qparse.cpp,v $
 * Revision 1.2  2007/01/11 14:49:52  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.1  2007/01/10 17:06:31  kuznets
 * initial revision
 *
 * ===========================================================================
 */
