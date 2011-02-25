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
* Author: Michael Kornbluh
*
* File Description:
*   Used to attach user functions to be called by the traversal.
*/

#include <ncbi_pch.hpp>

#include "traversal_attach_user_funcs_callback.hpp"

#include <serial/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   Serial_DataTool

BEGIN_NCBI_SCOPE

// CTraversalAttachUserFuncsCallback::TPatternVec CTraversalAttachUserFuncsCallback::kEmptyPatternVec;

CTraversalAttachUserFuncsCallback::CTraversalAttachUserFuncsCallback( 
    CTraversalSpecFileParser &spec_file_parser, 
    CTraversalNode::TNodeSet &nodesWithFunctions )
    : m_NodesWithFunctions(nodesWithFunctions)
{
    // go through every node ever created and put it into bins based on the typename and varname and class-name
    ITERATE( CTraversalNode::TNodeRawSet, node_iter, CTraversalNode::GetEveryNode() ) {

        // we can't match references
        if( x_NodeIsUnmatchable(**node_iter) ) {
            continue;
        }

        m_LeafToPossibleNodes[(*node_iter)->GetTypeName()]      .push_back( (*node_iter)->Ref() );
        m_LeafToPossibleNodes[(*node_iter)->GetVarName()]       .push_back( (*node_iter)->Ref() );
    }

    // now, go through every pattern and try to match it to nodes
    ITERATE( std::vector<CTraversalSpecFileParser::CDescFileNodeRef>, pattern_iter, spec_file_parser.GetDescFileNodes() ) {
        x_TryToAttachPattern( *pattern_iter );
    }
}

void CTraversalAttachUserFuncsCallback::x_TryToAttachPattern( CTraversalSpecFileParser::CDescFileNodeRef pattern )
{
    const string &last_node = pattern->GetPattern().back();
    bool pattern_was_used = false;
    bool except_pattern_was_used = false;

    // if there's no except_pattern, it's always considered used
    if( pattern->GetExceptPatterns().empty() ) {
        except_pattern_was_used = true;
    }

    NON_CONST_ITERATE( CTraversalNode::TNodeVec, node_iter, m_LeafToPossibleNodes[last_node] ) {
        if( x_PatternMatches( *node_iter, pattern->GetPattern().rbegin(), pattern->GetPattern().rend() ) ) {
            // if pattern matches, make sure none of the EXCEPT patterns match.
            if( x_AnyPatternMatches( *node_iter, pattern->GetExceptPatterns() ) ) {
                except_pattern_was_used = true;
            } else {
                // Actually attach the node (This could still fail and throw exception for certain cases,
                // but such failure is considered fatal)
                pattern_was_used = true;
                x_DoAttachment( *node_iter,pattern );
            }
        }
    }

    // warn the user on unused patterns or unused exceptions, since there's
    // a high chance the user made a typo
    if( ! pattern_was_used ) {
        ERR_POST_X(2, Warning << "Pattern was unused: " << pattern->ToString() );
    } else if( ! except_pattern_was_used ) {
        ERR_POST_X(2, Warning << "Pattern exception was unused: " << pattern->ToString() );
    }
}

bool 
CTraversalAttachUserFuncsCallback::x_PatternMatches( 
    CRef<CTraversalNode> node, TPatternIter pattern_start, TPatternIter pattern_end )
{
    // this func is recursive

    // skip over references
    if( x_NodeIsUnmatchable(*node) ) {
        ITERATE( CTraversalNode::TNodeSet, caller_iter, node->GetCallers() ) {
            if( x_PatternMatches( *caller_iter, pattern_start, pattern_end ) ) { // notice no " + 1"
                return true;
            }
        }
        return false;
    }

    // shouldn't happen
    _ASSERT( pattern_start != pattern_end );

    // bail if we don't match the current node
    const std::string &current_leaf = *pattern_start;
    if( current_leaf != "?" ) { // "?" matches anything
        if( current_leaf != node->GetVarName() && 
            current_leaf != node->GetTypeName() && 
            current_leaf != node->GetInputClassName() ) 
        {
            return false;
        }
    }

    // We're done; we matched everything
    if( (pattern_start + 1) == pattern_end ) {
        return true;
    }

    // recurse up callers.  success if any of them match
    ITERATE( CTraversalNode::TNodeSet, caller_iter, node->GetCallers() ) {
        if( x_PatternMatches( *caller_iter, pattern_start + 1, pattern_end ) ) { // notice the " + 1"
            return true;
        }
    }

    // couldn't match anything
    return false;
}

bool CTraversalAttachUserFuncsCallback::x_AnyPatternMatches( 
    CRef<CTraversalNode> node, 
    const CTraversalSpecFileParser::TPatternVec &patterns )
{
    // straightforward: just try to match any pattern
    ITERATE( CTraversalSpecFileParser::TPatternVec, pattern_iter, patterns ) {
        if( x_PatternMatches( node, pattern_iter->rbegin(), pattern_iter->rend() ) ) {
            return true;
        }
    }
    return false;
}

void CTraversalAttachUserFuncsCallback::x_DoAttachment( 
    CRef<CTraversalNode> node, 
    CTraversalSpecFileParser::CDescFileNodeRef pattern )
{
    // do the attachment that binds the user function to this node

    vector< CRef<CTraversalNode> > extra_arg_nodes;

    // for any extra args the node requires, find the node whose value this corresponds to.
    ITERATE( CTraversalSpecFileParser::TPatternVec, extra_arg_iter, pattern->GetArgPatterns() ) {
        CRef<CTraversalNode> arg_node = 
            x_TranslateArgToNode(node, pattern->GetPattern(), *extra_arg_iter);
        extra_arg_nodes.push_back( arg_node );
        arg_node->SetDoStoreArg();
    }

    // create and bind the user call
    CRef<CTraversalNode::CUserCall> user_call( 
        new CTraversalNode::CUserCall(pattern->GetFunc(), extra_arg_nodes ) );
    if( pattern->GetWhen() == CTraversalSpecFileParser::CDescFileNode::eWhen_afterCallees ) {
        node->AddPostCalleesUserCall( user_call );
    } else {
        node->AddPreCalleesUserCall( user_call );
    }

    // add the node to the set that have functions
    m_NodesWithFunctions.insert( node->Ref() );
}

CRef<CTraversalNode> CTraversalAttachUserFuncsCallback::x_TranslateArgToNode( 
    CRef<CTraversalNode> node, 
    const CTraversalSpecFileParser::TPattern &main_pattern,
    const CTraversalSpecFileParser::TPattern &extra_arg_pattern )
{
    // how many levels do we have to climb up to find the extra_arg_pattern?
    const int levels_to_climb = ( main_pattern.size() - extra_arg_pattern.size() );

    // climb up
    int level_up = 0;
    CRef<CTraversalNode> current_node = node;
    for( ; level_up < levels_to_climb; ++level_up ) {
        if( current_node->GetCallers().size() != 1 ) {
            // I'm not entirely sure this can even happen.
            string msg("when using extra args, the extra args ");
            msg += "must come from a 'direct' parent node. ";
            msg += "That is, there must be only ONE path from the node ";
            msg += "supplying the extra args to ";
            msg += "the node that needs the extra arg, with only one ";
            msg += "caller up to the top.  ";
            msg += "Main pattern: '";
            msg += NStr::Join(main_pattern, ".");
            msg += "', extra arg pattern: '";
            msg += NStr::Join(extra_arg_pattern, ".");
            msg += "'";
            throw runtime_error( msg );
        }
        current_node = *(current_node->GetCallers().begin());
        // skip unmatchable nodes
        if( x_NodeIsUnmatchable(*current_node) ) {
            --level_up;
        }
    }

    // make sure the node we reached matches the given pattern, which it should unless we've 
    // made a coding error.
    _ASSERT( x_PatternMatches( current_node, extra_arg_pattern.rbegin(), extra_arg_pattern.rend() ) );

    return current_node;
}

bool CTraversalAttachUserFuncsCallback::x_NodeIsUnmatchable( const CTraversalNode& node )
{
    if( (node.GetType() == CTraversalNode::eType_Reference) && ! node.GetCallees().empty() ) {
        const CTraversalNode& node_child = **node.GetCallees().begin();
        return ( x_UseRefOrChild( node, node_child ) == eRefChoice_ChildOnly );
    } else if( ! node.GetCallers().empty() ) {
        const CTraversalNode& node_parent = **node.GetCallers().begin();
        if( node_parent.GetType() == CTraversalNode::eType_Reference ) {
            return ( x_UseRefOrChild( node_parent, node ) == eRefChoice_RefOnly );
        }
    }

    return false;
}

CTraversalAttachUserFuncsCallback::ERefChoice 
CTraversalAttachUserFuncsCallback::x_UseRefOrChild(
    const CTraversalNode& parent_ref,
    const CTraversalNode& child )
{
    if( parent_ref.GetVarName() != child.GetVarName() ) {
        return eRefChoice_Both;
    }

    if( ! parent_ref.IsTemplate() && child.IsTemplate() ) {
        return eRefChoice_RefOnly;
    }

    // the usual case is to skip references
    return eRefChoice_ChildOnly;
}

END_NCBI_SCOPE

