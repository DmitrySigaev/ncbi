#ifndef AUTOPTR_HPP
#define AUTOPTR_HPP

template<typename T>
class AutoPtr {
public:
    AutoPtr(T* p = 0)
        : ptr(p)
        {
        }
    AutoPtr(const AutoPtr& p)
        : ptr(p.ptr)
        {
            p.ptr = 0;
        }

    ~AutoPtr()
        {
            delete ptr;
        }

    AutoPtr& operator=(const AutoPtr& p)
        {
            if ( ptr != p.ptr ) {
                delete ptr;
                ptr = p.ptr;
                p.ptr = 0;
            }
            return *this;
        }
    AutoPtr& operator=(T* p)
        {
            if ( ptr != p ) {
                delete ptr;
                ptr = p;
            }
            return *this;
        }

    T& operator*() const
        {
            return *ptr;
        }
    T* operator->() const
        {
            return ptr;
        }
    
    T* get() const
        {
            return ptr;
        }
    void release() const
        {
            ptr = 0;
        }

    operator bool() const
        {
            return ptr;
        }

private:
    mutable T* ptr;
};

template<typename T1, typename T2>
inline
void Assign(AutoPtr<T1>& t1, const AutoPtr<T2>& t2)
{
    t1 = t2.get();
    t2.release();
}

#endif
