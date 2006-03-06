#ifndef CORELIB___NCBI_CACHE__HPP
#define CORELIB___NCBI_CACHE__HPP
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
 * Author: Aleksey Grichenko
 *
 * File Description:
 *	 Generic cache.
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <set>
#include <map>


BEGIN_NCBI_SCOPE

/** @addtogroup Cache
 *
 * @{
 */


/// @file ncbi_cache.hpp
/// The NCBI C++ generic cache template


/////////////////////////////////////////////////////////////////////////////
///
///    Generic cache.
///


/// Internal structure to hold cache elements
template <class TKey, class TSize>
struct SCacheElement
{
    typedef TKey     TKeyType;
    typedef TSize    TSizeType;

    SCacheElement(void) : m_Weight(0), m_Order(0) {}
    SCacheElement(const TKeyType&  key,
                  const TSizeType& weight,
                  const TSizeType& order)
        : m_Key(key), m_Weight(weight), m_Order(order) {}

    TKeyType  m_Key;     // cache element key
    TSizeType m_Weight;  // element weight
    TSizeType m_Order;   // order of insertion
};


/// Compare cache elements by weight/order
template <class TCacheElementPtr>
struct CCacheElement_Less
{
    bool operator()(const TCacheElementPtr& x,
                    const TCacheElementPtr& y) const
        {
            _ASSERT(x  &&  y);
            if (x->m_Weight != y->m_Weight) {
                return x->m_Weight < y->m_Weight;
            }
            return x->m_Order < y->m_Order;
        }
};


/// Default (NOP) element remover
template <class TKey, class TValue>
class CCacheElement_Remover
{
public:
    static void RemoveElement(const TKey& key, TValue& value) {}
};


/// Fast cache lock - used by default in MT builds
class CCacheLock_FastMutex
{
public:
    typedef CFastMutex      TLock;
    typedef CFastMutexGuard TGuard;
};

/// Cache lock using CMutex. May be necessary if RemoveElement() makes
/// calls to the same cache object.
class CCacheLock_Mutex
{
public:
    typedef CMutex      TLock;
    typedef CMutexGuard TGuard;
};

/// NOP cache lock - provides no real MT-safety
class CCacheLock_Dummy
{
public:
    struct SVoid {
        ~SVoid(void) {}
    };
    typedef SVoid TLock;
    typedef SVoid TGuard;
};


/// Default cache lock
typedef CCacheLock_FastMutex TCacheLock_Default;


/// Cache traits template.
/// TKey and TValue define types stored in the cache.
/// TLock must define TGuard type and TLock subtypes so that a value of type
/// TLock can be used to initialize TGuard.
/// TSize is an integer type used for element indexing.
/// TRemover must provide static method called for each element to be
/// removed from the cache:
///   void RemoveElement(const TKey& key, TValue& value)
template <class            TKey,
          class            TValue,
          class TLock    = TCacheLock_Default,
          class TSize    = Uint4,
          class TRemover = CCacheElement_Remover<TKey, TValue> >
class CCacheTraits
{
public:
    typedef TKey     TKeyType;
    typedef TValue   TValueType;
    typedef TSize    TSizeType;
    /// Element remover
    typedef TRemover TRemoverType;
    /// MT-safety
    typedef TLock    TLockType;
};


/// Cache template. TKey and TValue define types stored in the cache.
template <class TKey,
          class TValue,
          class TTraits = CCacheTraits<TKey, TValue> >
class CCache
{
public:
    typedef typename TTraits::TKeyType          TKeyType;
    typedef typename TTraits::TValueType        TValueType;
    typedef typename TTraits::TSizeType         TSizeType;
    typedef SCacheElement<TKeyType, TSizeType>  TCacheElement;
    typedef TSizeType                           TWeight;
    typedef TSizeType                           TOrder;

    /// Create cache object with the given capacity
    CCache(TSizeType capacity);
    ~CCache(void);

    /// Result of element insertion
    enum EAddResult {
        eElement_Added,    ///< The element was added to the cache
        eElement_Replaced  ///< The element existed and was replaced
    };

    /// Add new element to the cache or replace the existing value.
    /// @param key
    ///   Element key
    /// @param value
    ///   Element value
    /// @param weight
    ///   Weight adjustment. The lifetime of each object in the cache
    ///   is proportional to its weight.
    /// @param result
    ///   Pointer to a variable to store operation result code to.
    TOrder Add(const TKeyType&   key,
               const TValueType& value,
               TWeight           weight = 1,
               EAddResult*       result = 0);

    /// Cache retrieval flag
    enum EGetFlag {
        eGet_Touch,   ///< Touch the object (move to the cache head)
        eGet_NoTouch  ///< Do not change the object's position
    };

    /// Get an object from the cache by its key.
    TValueType Get(const TKeyType& key, EGetFlag flag = eGet_Touch);

    /// Get current capacity of the cache (max allowed number of elements)
    TSizeType GetCapacity(void) const { return m_Capacity; }

    /// Set new capacity of the cache. The number of elements in the cache
    /// may be reduced to match the new capacity.
    /// @param new_capacity
    ///   new cache capacity, must be > 0.
    void SetCapacity(TSizeType new_capacity);

    /// Get current number of elements in the cache
    TSizeType GetSize(void) const { return m_CacheSet.size(); }

    /// Truncate the cache leaving at most new_size elements.
    /// Does not affect cache capacity. If new_size is zero
    /// all elements will be removed.
    void SetSize(TSizeType new_size);

private:
    struct SValueWithIndex {
        TCacheElement* m_CacheElement;
        TValueType     m_Value;
    };

    typedef CCacheElement_Less<TCacheElement*>   TCacheLess;
    typedef set<TCacheElement*, TCacheLess>      TCacheSet;
    typedef typename TCacheSet::iterator         TCacheSet_I;
    typedef map<TKeyType, SValueWithIndex>       TCacheMap;
    typedef typename TCacheMap::iterator         TCacheMap_I;
    typedef typename TTraits::TLockType          TLockTraits;
    typedef typename TLockTraits::TGuard         TGuardType;
    typedef typename TLockTraits::TLock          TLockType;
    typedef typename TTraits::TRemoverType       TRemoverType;

    // Get next counter value, adjust order of all elements if the counter
    // approaches its limit.
    TOrder x_GetNextCounter(void);
    void x_PackElementIndex(void);
    TCacheElement* x_InsertElement(const TKeyType& key, TWeight weight);
    void x_UpdateElement(TCacheElement* elem);
    void x_EraseElement(TCacheSet_I& set_iter, TCacheMap_I& map_iter);
    void x_EraseLast(void);
    TWeight x_GetBaseWeight(void) const
        {
            return m_CacheSet.empty() ? 0 : (*m_CacheSet.begin())->m_Weight;
        }

    TLockType m_Lock;
    TSizeType m_Capacity;
    TCacheSet m_CacheSet;
    TCacheMap m_CacheMap;
    TOrder    m_Counter;
};


/// Exception thrown by CCache
class NCBI_XNCBI_EXPORT CCacheException : public CException
{
public:
    enum EErrCode {
        eIndexOverflow,    ///< Element index overflow
        eWeightOverflow,   ///< Element weight overflow
        eOtherError
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CCacheException, CException);
};


/////////////////////////////////////////////////////////////////////////////
//
//  CCache<> implementation
//

template <class TKey, class TValue, class TTraits>
CCache<TKey, TValue, TTraits>::CCache(TSizeType capacity)
    : m_Capacity(capacity),
      m_Counter(0)
{
    _ASSERT(capacity > 0);
}


template <class TKey, class TValue, class TTraits>
CCache<TKey, TValue, TTraits>::~CCache(void)
{
    TGuardType guard(m_Lock);

    while ( !m_CacheSet.empty() ) {
        x_EraseLast();
    }
    _ASSERT(m_CacheMap.empty());
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::x_EraseElement(TCacheSet_I& set_iter,
                                                   TCacheMap_I& map_iter)
{
    _ASSERT(set_iter != m_CacheSet.end());
    _ASSERT(map_iter != m_CacheMap.end());
    TCacheElement* next = *set_iter;
    _ASSERT(next);
    TRemoverType::RemoveElement(map_iter->first, map_iter->second.m_Value);
    m_CacheMap.erase(map_iter);
    m_CacheSet.erase(set_iter);
    delete next;
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::x_EraseLast(void)
{
    _ASSERT(!m_CacheSet.empty());
    TCacheSet_I set_iter = m_CacheSet.begin();
    TCacheMap_I map_iter = m_CacheMap.find((*set_iter)->m_Key);
    x_EraseElement(set_iter, map_iter);
}


template <class TKey, class TValue, class TTraits>
typename CCache<TKey, TValue, TTraits>::TCacheElement*
CCache<TKey, TValue, TTraits>::x_InsertElement(const TKeyType& key,
                                               TWeight         weight)
{
    if (weight == 0) {
        weight = 1;
    }
    TWeight adjusted_weight = weight + x_GetBaseWeight();
    if (adjusted_weight < weight) {
        x_PackElementIndex();
        adjusted_weight = weight + x_GetBaseWeight();
        if (adjusted_weight < weight) {
            NCBI_THROW(CCacheException, eWeightOverflow,
                "Cache element weight overflow");
        }
    }
    TCacheElement* elem = new TCacheElement(key, adjusted_weight,
        x_GetNextCounter());
    m_CacheSet.insert(elem);
    return elem;
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::x_UpdateElement(TCacheElement* elem)
{
    _ASSERT(elem);
    TCacheSet_I it = m_CacheSet.find(elem);
    _ASSERT(it != m_CacheSet.end());
    _ASSERT(*it == elem);
    m_CacheSet.erase(it);
    elem->m_Order = x_GetNextCounter();
    if (TWeight(elem->m_Weight + 1) <= 0) {
        x_PackElementIndex();
    }
    elem->m_Weight++;
    m_CacheSet.insert(elem);
}


template <class TKey, class TValue, class TTraits>
typename CCache<TKey, TValue, TTraits>::TOrder
CCache<TKey, TValue, TTraits>::x_GetNextCounter(void)
{
    if (TSizeType(m_Counter + 1) <= 0) {
        x_PackElementIndex();
    }
    return ++m_Counter;
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::x_PackElementIndex(void)
{
    // Overflow detected - adjust orders
    TOrder order_shift = m_Counter - 1;
    if ( !m_CacheSet.empty() ) {
        TWeight weight_shift = (*m_CacheSet.begin())->m_Weight - 1;
        TWeight weight_max = weight_shift;
        TOrder order_min = 0;
        ITERATE(typename TCacheSet, it, m_CacheSet) {
            TCacheElement* e = *it;
            if (e->m_Order < order_shift  &&  e->m_Order > order_min) {
                if (e->m_Order >= (order_shift + order_min)/2) {
                    order_shift = e->m_Order;
                }
                else {
                    order_min = e->m_Order;
                }
            }
            if (e->m_Weight > weight_max) {
                weight_max = e->m_Weight;
            }
        }
        if (order_shift - order_min < 2) {
            // Can not pack orders, try slow method
            typedef set<TOrder> TOrderSet;
            TOrderSet orders;
            ITERATE(typename TCacheSet, it, m_CacheSet) {
                orders.insert((*it)->m_Order);
            }
            TOrder rg_from = 0;
            TOrder rg_to = 0;
            TOrder last = 1;
            // Find the longest unused range
            ITERATE(typename TOrderSet, it, orders) {
                if (*it - last > rg_to - rg_from) {
                    rg_from = last;
                    rg_to = *it;
                }
                last = *it;
            }
            if (rg_to - rg_from < 2) {
                NCBI_THROW(CCacheException, eIndexOverflow,
                           "Cache element index overflow");
            }
            order_min = rg_from;
            order_shift = rg_to;
        }
        order_shift -= order_min;
        if (weight_shift <= 1  &&  weight_max + 1 <= 0) {
            // Can not pack weights
            NCBI_THROW(CCacheException, eWeightOverflow,
                       "Cache element weight overflow");
        }
        order_shift--;
        // set<> elements are modified but the order is not changed
        NON_CONST_ITERATE(typename TCacheSet, it, m_CacheSet) {
            TCacheElement* e = *it;
            if (e->m_Order > order_min) {
                e->m_Order -= order_shift;
            }
            e->m_Weight -= weight_shift;
        }
    }
    m_Counter -= order_shift;
}


template <class TKey, class TValue, class TTraits>
typename CCache<TKey, TValue, TTraits>::TOrder
CCache<TKey, TValue, TTraits>::Add(const TKeyType&   key,
                                   const TValueType& value,
                                   TWeight           weight,
                                   EAddResult*       result)
{
    TGuardType guard(m_Lock);

    TCacheMap_I it = m_CacheMap.find(key);
    if (it != m_CacheMap.end()) {
        TCacheSet_I set_it = m_CacheSet.find(it->second.m_CacheElement);
        x_EraseElement(set_it, it);
        if ( result ) {
            *result = eElement_Replaced;
        }
    }
    else if ( result ) {
        *result = eElement_Added;
    }

    while (GetSize() >= m_Capacity) {
        x_EraseLast();
    }

    SValueWithIndex& map_val = m_CacheMap[key];
    map_val.m_CacheElement = x_InsertElement(key, weight);
    map_val.m_Value = value;
    _ASSERT(map_val.m_CacheElement);
    return map_val.m_CacheElement->m_Order;
}


template <class TKey, class TValue, class TTraits>
typename CCache<TKey, TValue, TTraits>::TValueType
CCache<TKey, TValue, TTraits>::Get(const TKeyType& key,
                                   EGetFlag flag)
{
    TGuardType guard(m_Lock);

    TCacheMap_I it = m_CacheMap.find(key);
    if (it == m_CacheMap.end()) {
        return TValue();
    }
    if (flag == eGet_Touch) {
        x_UpdateElement(it->second.m_CacheElement);
    }
    return it->second.m_Value;
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::SetCapacity(TSizeType new_capacity)
{
    if (new_capacity <= 0) {
        NCBI_THROW(CCacheException, eOtherError,
                   "Cache capacity must be positive");
    }
    TGuardType guard(m_Lock);
    while (GetSize() > new_capacity) {
        x_EraseLast();
    }
    m_Capacity = new_capacity;
}


template <class TKey, class TValue, class TTraits>
void CCache<TKey, TValue, TTraits>::SetSize(TSizeType new_size)
{
    TGuardType guard(m_Lock);
    while (GetSize() > new_size) {
        x_EraseLast();
    }
}


/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/03/06 18:28:25  grichenk
 * Moved ncbi_cache from corelib to util.
 *
 * Revision 1.1  2006/02/28 16:24:05  grichenk
 * Initial revision
 *
 *
 * ==========================================================================
 */

#endif  // CORELIB___NCBI_CACHE__HPP
