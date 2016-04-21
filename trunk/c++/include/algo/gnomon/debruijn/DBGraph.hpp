#ifndef _DeBruijn_Graph_
#define _DeBruijn_Graph_

#include <iostream>
#include "Integer.hpp"
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <algo/gnomon/gnomon_model.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(gnomon);
USING_SCOPE(boost);
USING_SCOPE(std);

namespace DeBruijn {

    template <typename T1, typename T2, typename T3, typename T4, typename T5> class CKmerCountTemplate {
    public:
        typedef variant<vector<pair<T1,size_t>>, vector<pair<T2,size_t>>, vector<pair<T3,size_t>>, vector<pair<T4,size_t>>, vector<pair<T5,size_t>> > Type;

        CKmerCountTemplate(int kmer_len = 0) : m_kmer_len(kmer_len) {
            if(m_kmer_len > 0) {
                init_container();
            }
        }
        size_t Size() const {
            return apply_visitor(container_size(), m_container);
        }
        void Reserve(size_t rsrv) { apply_visitor(reserve(rsrv), m_container); }
        void Clear() { apply_visitor(clear(), m_container); }
        size_t Capacity() const { return apply_visitor(container_capacity(), m_container); }
        size_t ElementSize() const { return apply_visitor(element_size(), m_container); }
        void PushBack(const TKmer& kmer, size_t count) { 
            if(m_kmer_len == 0) {
                cerr << "Can't insert in not initialized container" << endl;
                exit(1);
            }
            apply_visitor(push_back(kmer, count), m_container); 
        }
        void PushBackElementsFrom(const CKmerCountTemplate& other) { 
            if(m_kmer_len == 0) {
                cerr << "Can't insert in not initialized container" << endl;
                exit(1);
            }
            return apply_visitor(push_back_elements(), m_container, other.m_container); 
        }
        // 32 bit count; 8 bit branching; 1 bit 'visited'; 7 bit not used yet; 16 bit +/-
        void UpdateCount(size_t count, size_t index) { apply_visitor(update_count(count, index), m_container); }
        size_t GetCount(size_t index) const { return apply_visitor(get_count(index), m_container); }
        pair<TKmer,size_t> GetKmerCount(size_t index) const { return apply_visitor(get_kmer_count(index), m_container); }
        size_t Find(const TKmer& kmer) const { return apply_visitor(find_kmer(kmer), m_container); }
        int KmerLen() const { return m_kmer_len; }
        void Sort() { apply_visitor(container_sort(), m_container); }
        void SortAndExtractUniq(int min_count, CKmerCountTemplate& uniq) {
            uniq = CKmerCountTemplate(m_kmer_len); // init
            Sort();
            apply_visitor(extract_uniq(min_count), m_container, uniq.m_container);
        }
        void SortAndUniq(int min_count) {
            Sort();
            apply_visitor(uniq(min_count), m_container);
        }
        void MergeTwoSorted(const CKmerCountTemplate& other) {
            if(m_kmer_len != other.KmerLen()) {
                cerr << "Can't merge different kmers" << endl;
                exit(1);
            }
            apply_visitor(merge_sorted(), m_container, other.m_container);
        }
        void Swap(CKmerCountTemplate& other) {
            swap(m_kmer_len, other.m_kmer_len);
            apply_visitor(swap_with_other(), m_container, other.m_container);    
        }
        void Save(ostream& out) const { 
            out.write(reinterpret_cast<const char*>(&m_kmer_len), sizeof(m_kmer_len));
            apply_visitor(save(out), m_container);
        }
        void Load(istream& in) {
            in.read(reinterpret_cast<char*>(&m_kmer_len), sizeof(m_kmer_len));
            init_container();
            apply_visitor(load(in), m_container);
        }

    private:
        void init_container() {
            int p = (m_kmer_len+31)/32;
            if(p <= PREC_1)
                m_container = vector<pair<T1,size_t>>();
            else if(p <= PREC_2)
                m_container = vector<pair<T2,size_t>>();
            else if(p <= PREC_3)
                m_container = vector<pair<T3,size_t>>();
            else if(p <= PREC_4)
                m_container = vector<pair<T4,size_t>>();
            else if(p <= PREC_5)
                m_container = vector<pair<T5,size_t>>();
            else
                throw runtime_error("Not supported kmer length");
        }

        struct find_kmer : public static_visitor<size_t> { 
            find_kmer(const TKmer& k) : kmer(k) {}
            template <typename T> size_t operator()(const T& v) const { 
                typedef typename T::value_type pair_t;
                typedef typename pair_t::first_type large_t;
                auto it = lower_bound(v.begin(), v.end(), kmer.get<large_t>(), [](const pair_t& element, const large_t& target){ return element.first < target; });
                if(it == v.end() || it->first != kmer.get<large_t>())
                    return v.size();
                else
                    return it-v.begin();
            } 
            const TKmer& kmer;
        };
        struct reserve : public static_visitor<> { 
            reserve(size_t r) : rsrv(r) {}
            template <typename T> void operator() (T& v) const { v.reserve(rsrv); }
            size_t rsrv;
        };
        struct container_size : public static_visitor<size_t> { template <typename T> size_t operator()(const T& v) const { return v.size();} };
        struct container_capacity : public static_visitor<size_t> { template <typename T> size_t operator()(const T& v) const { return v.capacity();} };
        struct element_size : public static_visitor<size_t> { template <typename T> size_t operator()(const T& v) const { return sizeof(typename  T::value_type);} };
        struct clear : public static_visitor<> { template <typename T> void operator()(T& v) const { v.clear();} };
        struct push_back : public static_visitor<> { 
            push_back(const TKmer& k, size_t c) : kmer(k), count(c) {}
            template <typename T> void operator() (T& v) const {
                typedef typename  T::value_type::first_type large_t;
                v.push_back(make_pair(kmer.get<large_t>(), count)); 
            }
            const TKmer& kmer;
            size_t count;
        };
        struct push_back_elements : public static_visitor<> {
            template <typename T> void operator() (T& a, const T& b) const { a.insert(a.end(), b.begin(), b.end()); }
            template <typename T, typename U> void operator() (T& a, const U& b) const { cerr << "Can't copy from different type container" << endl; exit(1); }
        };
        struct merge_sorted : public static_visitor<> {
            template <typename T> void operator() (T& a, const T& b) const { 
                T merged;
                merged.reserve(a.size()+b.size());
                merge(a.begin(), a.end(), b.begin(), b.end(), back_inserter(merged));
                merged.swap(a); 
            }
            template <typename T, typename U> void operator() (T& a, const U& b) const { cerr << "This is not happening!" << endl; exit(1); }
        };
        struct update_count : public static_visitor<> {
            update_count(size_t c, size_t i) : count(c), index(i) {}
            template <typename T> void operator() (T& v) const { v[index].second = count; }  
            size_t count;
            size_t index;
        };
        struct get_count : public static_visitor<size_t> {
            get_count(size_t i) : index(i) {}
            template <typename T> size_t operator() (T& v) const { return v[index].second; }        
            size_t index;
        };
        struct get_kmer_count : public static_visitor<pair<TKmer,size_t>> {
            get_kmer_count(size_t i) : index(i) {}
            template <typename T> pair<TKmer,size_t> operator() (T& v) const { return make_pair(TKmer(v[index].first), v[index].second); }        
            size_t index;
        };
        struct container_sort : public static_visitor<> { template <typename T> void operator() (T& v) const { sort(v.begin(), v.end()); }};
        struct swap_with_other : public static_visitor<> { 
            template <typename T> void operator() (T& a, T& b) const { a.swap(b); }
            template <typename T, typename U> void operator() (T& a, U& b)  const { cerr << "Can't swap different type containers" << endl; exit(1); } 
        };
        struct uniq : public static_visitor<> {
            uniq(int mc) : min_count(mc) {}
            template <typename T> void operator() (T& v) const {
                typedef typename T::iterator iter_t;
                iter_t nextp = v.begin();
                for(iter_t ip = v.begin(); ip != v.end(); ) {
                    iter_t workp = ip;
                    while(++ip != v.end() && workp->first == ip->first)
                        workp->second += ip->second;   // accumulate all 8 bytes; we assume that count will not spill into higher half
                    if((uint32_t)workp->second >= min_count)
                        *nextp++ = *workp;
                }
                v.erase(nextp, v.end());
            }

            int min_count;
        };
        struct extract_uniq : public static_visitor<> {
            extract_uniq(int mc) : min_count(mc) {}
            template <typename T> void operator() (T& a, T& b) const {
                if(a.empty()) return;
                size_t num = 1;
                uint32_t count = a[0].second;  // count only 4 bytes!!!!!!
                for(size_t i = 1; i < a.size(); ++i) {
                    if(a[i-1].first < a[i].first) {
                        if(count >= min_count)
                            ++num;
                        count = a[i].second;
                    } else {
                        count += a[i].second;
                    }
                }
                if(count < min_count)
                    --num;
                b.reserve(num+1);
                b.push_back(a[0]);
                for(size_t i = 1; i < a.size(); ++i) {
                    if(b.back().first < a[i].first) {
                        if((uint32_t)b.back().second < min_count)
                            b.pop_back();
                        b.push_back(a[i]);
                    } else {
                        b.back().second += a[i].second;  // accumulate all 8 bytes; we assume that count will not spill into higher half
                    }
                }
                if((uint32_t)b.back().second < min_count)
                    b.pop_back();
            }
            template <typename T, typename U> void operator() (T& a, U& b) const { cerr << "This is not happening!" << endl; exit(1); }

            int min_count;
        };
        struct save : public static_visitor<> {
            save(ostream& out) : os(out) {}
            template <typename T> void operator() (T& v) const {
                size_t num = v.size();
                os.write(reinterpret_cast<const char*>(&num), sizeof num); 
                if(num > 0)
                    os.write(reinterpret_cast<const char*>(&v[0]), num*sizeof(v[0]));
            }
            ostream& os;
        };
        struct load : public static_visitor<> {
            load(istream& in) : is(in) {}
            template <typename T> void operator() (T& v) const {
                size_t num;
                is.read(reinterpret_cast<char*>(&num), sizeof num);
                if(num > 0) {
                    v.resize(num);
                    is.read(reinterpret_cast<char*>(&v[0]), num*sizeof(v[0]));
                }
            }
            istream& is;
        };

        Type m_container;
        int m_kmer_len;
    };
    typedef CKmerCountTemplate <INTEGER_TYPES> TKmerCount;

    template <typename T1, typename T2, typename T3, typename T4, typename T5, typename V> class CKmerMapTemplate {
    public:
        struct kmer_hash {
            template<typename T> 
            size_t operator() (const T& kmer) const { return kmer.oahash(); }
        };
        typedef variant<unordered_map<T1,V,kmer_hash>, unordered_map<T2,V,kmer_hash>, unordered_map<T3,V,kmer_hash>, unordered_map<T4,V,kmer_hash>, unordered_map<T5,V,kmer_hash>> Type;
        CKmerMapTemplate(int kmer_len = 0) : m_kmer_len(kmer_len) {
            if(m_kmer_len > 0) {
                init_container();
            }
        }
        size_t Size() const {
            return apply_visitor(container_size(), m_container);
        }
        void Reserve(size_t rsrv) { apply_visitor(reserve(rsrv), m_container); }
        V& operator[] (const TKmer& kmer) { 
            if(m_kmer_len == 0) {
                cerr << "Can't insert in not initialized container" << endl;
                exit(1);
            }
            return apply_visitor(mapper(kmer), m_container);
        }
        V* Find(const TKmer& kmer) { return apply_visitor(find(kmer), m_container); }
        int KmerLen() const { return m_kmer_len; }

    private:
        void init_container() {
            int p = (m_kmer_len+31)/32;
            if(p <= PREC_1)
                m_container = unordered_map<T1,V,kmer_hash>();
            else if(p <= PREC_2)
                m_container = unordered_map<T2,V,kmer_hash>();
            else if(p <= PREC_3)
                m_container = unordered_map<T3,V,kmer_hash>();
            else if(p <= PREC_4)
                m_container = unordered_map<T4,V,kmer_hash>();
            else if(p <= PREC_5)
                m_container = unordered_map<T5,V,kmer_hash>();
            else
                throw runtime_error("Not supported kmer length");
        }
        struct container_size : public static_visitor<size_t> { template <typename T> size_t operator()(const T& v) const { return v.size();} };
        struct reserve : public static_visitor<> { 
            reserve(size_t r) : rsrv(r) {}
            template <typename T> void operator() (T& v) const { v.reserve(rsrv); }
            size_t rsrv;
        };
        struct mapper : public static_visitor<V&> { 
            mapper(const TKmer& k) : kmer(k) {}
            template <typename T> V& operator()(T& v) const { 
                typedef typename  T::key_type large_t;
                return v[kmer.get<large_t>()];
            } 
            const TKmer& kmer;
        };
        struct find : public static_visitor<V*> {
            find(const TKmer& k) : kmer(k) {}
            template <typename T> V* operator()(T& v) const { 
                typedef typename  T::key_type large_t;
                typename T::iterator it = v.find(kmer.get<large_t>());
                if(it != v.end())
                    return &(it->second);
                else
                    return 0;
            } 
            const TKmer& kmer;
        };

        Type m_container;
        int m_kmer_len;
    };
    template <typename V>
    using  TKmerMap = CKmerMapTemplate<INTEGER_TYPES, V>;

    typedef vector<pair<int,size_t>> TBins;
    int FindValleyAndPeak(const TBins& bins, int rlimit) {
        int SLOPE_LEN = 5;
        int peak = min(rlimit,(int)bins.size()-SLOPE_LEN-1);
        while(peak >= SLOPE_LEN) {
            bool maxim = true;
            for(int i = 1; i <= SLOPE_LEN && maxim; ++i) 
                maxim = bins[peak+i].second < bins[peak].second;
            for(int i = 1; i <= SLOPE_LEN && maxim; ++i) 
                maxim = bins[peak-i].second < bins[peak].second;
            if(maxim)
                break;
            --peak;
        }

        if(peak < SLOPE_LEN)
            return -1;

        int valley = 0;
        for(int i = 1; i <= peak; ++i) {
            if(bins[i].second < bins[valley].second)
                valley = i;
        }
        if(valley == peak)
            return -1;

        for(int i = valley; i < (int)bins.size(); ++i) {
            if(bins[i].second > bins[peak].second)
                peak = i;
        }
        
        if(bins[valley].second < 0.7*bins[peak].second)
            return valley;
        else            
            return -1;
    }

    pair<int,int> HistogramRange(const TBins& bins) {  // returns <valley,rlimit>
        unsigned MIN_NUM = 100;
        size_t gsize = 0;
        for(auto& bin : bins) {
            if(bin.second >= MIN_NUM) 
                gsize += bin.first*bin.second;
        }

        int rl = 0;
        size_t gs = 0;
        for(auto& bin : bins) {
            gs += bin.first*bin.second;
            ++rl;
            if(gs > 0.8*gsize)
                break;
        }

        int valley = -1;
        int rlimit = rl;
        size_t genome = 0;

        while(true) {
            int v = FindValleyAndPeak(bins, rl);

            size_t g = 0;
            for(int i = max(0, v); i <= rl; ++i)
                g += bins[i].second;

            if((v >= 0 && g > genome) || g > 10*genome) {
                valley = v;
                rlimit = rl;
                genome = g;
                //                cerr << valley << " " << rlimit << " " << genome << endl;
            }

            if(v < 0)
                break;
            rl = v;
        }
        
        return make_pair(valley, rlimit);
    }

    class CDBGraph {
    public:

        typedef size_t Node;
        struct Successor {
            Successor(const Node& node, char c) : m_node(node), m_nt(c) {}
            Node m_node;
            char m_nt;
        };

        CDBGraph(const TKmerCount& kmers, const TBins& bins, bool is_stranded) : m_graph_kmers(kmers.KmerLen()), m_bins(bins), m_is_stranded(is_stranded) {
            m_graph_kmers.PushBackElementsFrom(kmers);
            string max_kmer(m_graph_kmers.KmerLen(), bin2NT[3]);
            m_max_kmer = TKmer(max_kmer);
        }

        CDBGraph(istream& in) {
            m_graph_kmers.Load(in);
            string max_kmer(m_graph_kmers.KmerLen(), bin2NT[3]);
            m_max_kmer = TKmer(max_kmer);

            int bin_num;
            in.read(reinterpret_cast<char*>(&bin_num), sizeof bin_num);
            for(int i = 0; i < bin_num; ++i) {
                pair<int, size_t> bin;
                in.read(reinterpret_cast<char*>(&bin), sizeof bin);
                m_bins.push_back(bin);
            }

            in.read(reinterpret_cast<char*>(&m_is_stranded), sizeof m_is_stranded);
        }

        // 0 for not contained kmers    
        // positive even numbers for direct kmers   
        // positive odd numbers for reversed kmers  
        Node GetNode(const TKmer& kmer) const {
            TKmer rkmer = revcomp(kmer, KmerLen());
            if(kmer < rkmer) {
                size_t index = m_graph_kmers.Find(kmer);
                return (index == GraphSize() ? 0 : 2*(index+1));
            } else {
                size_t index = m_graph_kmers.Find(rkmer);
                return (index == GraphSize() ? 0 : 2*(index+1)+1);
            }
        }
        Node GetNode(const string& kmer_seq) const {
            if(kmer_seq.find_first_not_of("ACGT") != string::npos || (int)kmer_seq.size() != KmerLen())   // invalid kmer
                return 0;
            TKmer kmer(kmer_seq);
            return GetNode(kmer);
        }

        // for all access with Node there is NO check that node is in range !!!!!!!!
        int Abundance(const Node& node) const {
            if(node == 0)
                return 0;
            else
                return m_graph_kmers.GetKmerCount(node/2-1).second;  // atomatically clips branching information!
        }
        // 32 bit count; 8 bit branching; 1 bit 'visited'; 7 bit not used yet; 16 bit +/-
        double MinFraction(const Node& node) const {
            double plusf = PlusFraction(node);
            return min(plusf,1-plusf);
        }
        double PlusFraction(const Node& node) const {
            double plusf = double(m_graph_kmers.GetKmerCount(node/2-1).second >> 48)/numeric_limits<uint16_t>::max();
            if(node%2)
                plusf = 1-plusf;
            return plusf;
        }
        TKmer GetNodeKmer(const Node& node) const {
            if(node%2 == 0) 
                return m_graph_kmers.GetKmerCount(node/2-1).first;
            else
                return revcomp(m_graph_kmers.GetKmerCount(node/2-1).first, KmerLen());
        }
        string GetNodeSeq(const Node& node) const {
            return GetNodeKmer(node).toString(KmerLen());
        }

        void SetVisited(const Node& node) {
            m_graph_kmers.UpdateCount(m_graph_kmers.GetCount(node/2-1) | (uint64_t(1) << 40), node/2-1);
        }

        void ClearVisited(const Node& node) {
            m_graph_kmers.UpdateCount(m_graph_kmers.GetCount(node/2-1) & ~(uint64_t(1) << 40), node/2-1);
        }

        bool IsVisited(const Node& node) const {
            return (m_graph_kmers.GetCount(node/2-1) & (uint64_t(1) << 40));
        }
        
        vector<Successor> GetNodeSuccessors(const Node& node) const {
            vector<Successor> successors;
            if(!node)
                return successors;

            uint8_t branch_info = (m_graph_kmers.GetCount(node/2-1) >> 32);
            bitset<4> branches(node%2 ? (branch_info >> 4) : branch_info);
            if(branches.count()) {
                TKmer shifted_kmer = (GetNodeKmer(node) << 2) & m_max_kmer;
                for(int nt = 0; nt < 4; ++nt) {
                    if(branches[nt]) {
                        Node successor = GetNode(shifted_kmer + TKmer(KmerLen(), nt));
                        successors.push_back(Successor(successor, bin2NT[nt]));
                    }
                }
            }

            return successors;            
        }

        static Node ReverseComplement(Node node) {
            if(node != 0)
                node = (node%2 == 0 ? node+1 : node-1);

            return node;
        }

        int KmerLen() const { return m_graph_kmers.KmerLen(); }
        size_t GraphSize() const { return m_graph_kmers.Size(); }
        size_t ElementSize() const { return m_graph_kmers.ElementSize(); }
        bool GraphIsStranded() const { return m_is_stranded; }

        int HistogramMinimum() const {
            pair<int,int> r = HistogramRange(m_bins);
            if(r.first < 0)
                return 0;
            else 
                return m_bins[r.first].first;
        }

    private:

        TKmerCount m_graph_kmers;     // only the minimal kmers are stored  
        TKmer m_max_kmer;             // contains 1 in all kmer_len bit positions  
        TBins m_bins;
        bool m_is_stranded;
    };

    class CReadHolder {
    public:
        CReadHolder() :  m_total_seq(0), m_front_shift(0) {};
        void PushBack(const string& read) {
            int shift = (m_total_seq*2 + m_front_shift)%64;
            for(int i = (int)read.size()-1; i >= 0; --i) {  // put backward for kmer compatibility
                if(shift == 0)
                    m_storage.push_back(0);
                m_storage.back() += ((find(bin2NT.begin(), bin2NT.end(),  read[i]) - bin2NT.begin()) << shift);
                shift = (shift+2)%64;
            }
            m_read_length.push_back(read.size());
            m_total_seq += read.size();
        }
        void PopFront() {
            m_total_seq -= m_read_length.front();
            if(m_total_seq == 0) {
                Clear();
            } else {
                int nextp = m_front_shift+2*m_read_length.front();
                m_read_length.pop_front();
                m_front_shift = nextp%64;
                for(int num = nextp/64; num > 0; --num)
                    m_storage.pop_front();
            }
        }
        void Swap(CReadHolder& other) {
            swap(m_storage, other.m_storage);
            swap(m_read_length, other.m_read_length);
            swap(m_total_seq, other.m_total_seq);
            swap(m_front_shift, other. m_front_shift);
        }
        void Clear() { CReadHolder().Swap(*this); }
        size_t TotalSeq() const { return m_total_seq; }
        size_t MaxLength() const { 
            if(m_read_length.empty())
                return 0;
            else
                return *max_element(m_read_length.begin(), m_read_length.end()); 
        }
        size_t KmerNum(unsigned kmer_len) const {
            size_t num = 0;
            if(m_read_length.empty())
                return num;
            for(auto l : m_read_length) {
                if(l >= kmer_len)
                    num += l-kmer_len+1;
            }
            return num;
        }
        size_t ReadNum() const { return m_read_length.size(); }

        size_t NXX(double xx) const {
            vector<uint32_t> read_length(m_read_length.begin(), m_read_length.end());
            sort(read_length.begin(), read_length.end());
            size_t nxx = 0;
            size_t len = 0;
            for(int j = (int)read_length.size()-1; j >= 0 && len < xx*m_total_seq; --j) {
                nxx = read_length[j];
                len += read_length[j];
            }
            
            return nxx;
        }
        size_t N50() const { return NXX(0.5); }

        class kmer_iterator {
        public:
            kmer_iterator(int kmer_len, const CReadHolder& rholder, size_t position = 0, size_t position_in_read = 0, size_t read = 0) : m_kmer_len(kmer_len), m_readholder(rholder), m_position(position), m_position_in_read(position_in_read), m_read(read) {
                SkipShortReads();
            }

            TKmer operator*() const {
                TKmer kmer(m_kmer_len, 0);
                uint64_t* guts = (uint64_t*)kmer.getPointer();
                unsigned precision = kmer.getSize()/64;
                int front_shift = m_readholder.m_front_shift;

                size_t first_word = (m_position+front_shift)/64;                // first, maybe partial, word in deque
                size_t end_word = (m_position+front_shift+2*m_kmer_len-1)/64+1; // word after last
                int shift = (m_position+front_shift)%64;
                if(shift == 0) {
                    copy(m_readholder.m_storage.begin()+first_word, m_readholder.m_storage.begin()+end_word, guts);
                } else {
                    if(end_word-first_word <= precision) {
                        copy(m_readholder.m_storage.begin()+first_word, m_readholder.m_storage.begin()+end_word, guts);
                        kmer = (kmer >> shift);
                    } else {
                        copy(m_readholder.m_storage.begin()+first_word+1, m_readholder.m_storage.begin()+end_word, guts);
                        kmer = (kmer << 64-shift);                // make space for first partial word
                        *guts += m_readholder.m_storage[first_word] >> shift;
                    }
                }
                unsigned prc = (m_kmer_len+31)/32;                   // rounded up number of 8-byte words in kmer
                int partial_part_bits = 2*(m_kmer_len%32);
                if(partial_part_bits > 0) {
                    uint64_t mask = (uint64_t(1) << partial_part_bits) - 1;
                    guts[prc-1] &= mask;
                }
                if(precision > prc)
                    guts[prc] = 0;

                return kmer;
            }

            kmer_iterator& operator++() {
                if(m_position == 2*(m_readholder.m_total_seq-m_kmer_len)) {
                    m_position = 2*m_readholder.m_total_seq;
                    return *this;
                }

                m_position += 2;
                if(++m_position_in_read == m_readholder.m_read_length[m_read]-m_kmer_len+1) {
                    m_position += 2*(m_kmer_len-1);
                    ++m_read;
                    m_position_in_read = 0;
                    SkipShortReads();
                } 

                return *this;
            }
            friend bool operator==(kmer_iterator const& li, kmer_iterator const& ri) { return li.m_position == ri.m_position; }
            friend bool operator!=(kmer_iterator const& li, kmer_iterator const& ri) { return li.m_position != ri.m_position; }

        private:
            void SkipShortReads() {
                while(m_position < 2*m_readholder.m_total_seq && m_read < m_readholder.m_read_length.size() && m_readholder.m_read_length[m_read] < m_kmer_len)
                    m_position += 2*m_readholder.m_read_length[m_read++];                               
            }
            uint32_t m_kmer_len;
            const CReadHolder& m_readholder;
            size_t m_position;           // BIT num in concatenated sequence
            uint32_t m_position_in_read; // SYMBOL in read
            size_t m_read;               // read number
        };
        kmer_iterator kend() const { return kmer_iterator(0, *this, 2*m_total_seq); }
        kmer_iterator kbegin(int kmer_len) const { return kmer_iterator(kmer_len, *this); }

        class string_iterator {
        public:
            string_iterator(const CReadHolder& rholder, size_t position = 0, size_t read = 0) : m_readholder(rholder), m_position(position), m_read(read) {}

            string operator*() const {
                string read;
                int read_length = m_readholder.m_read_length[m_read];
                size_t position = m_position+m_readholder.m_front_shift;
                for(int i = 0; i < read_length; ++i) {
                    read.push_back(bin2NT[(m_readholder.m_storage[position/64] >> position%64) & 3]);
                    position += 2;
                }
                reverse(read.begin(), read.end());
                return read;
            }
            string_iterator& operator++() {
                if(m_position == 2*m_readholder.m_total_seq)
                    return *this;
                m_position +=  2*m_readholder.m_read_length[m_read++]; 
                return *this;
            }
            size_t ReadLen() const { return m_readholder.m_read_length[m_read]; }
            kmer_iterator KmersForRead(int kmer_len) const {
                if(kmer_len <= (int)m_readholder.m_read_length[m_read])
                    return kmer_iterator(kmer_len, m_readholder, m_position, 0, m_read); 
                else
                    return m_readholder.kend();
            }
            friend bool operator==(string_iterator const& li, string_iterator const& ri) { return li.m_position == ri.m_position; }
            friend bool operator!=(string_iterator const& li, string_iterator const& ri) { return li.m_position != ri.m_position; }
        

        private:
            const CReadHolder& m_readholder;
            size_t m_position;
            size_t m_read;
        };
        string_iterator send() const { return string_iterator(*this, 2*m_total_seq, m_read_length.size()); }
        string_iterator sbegin() const { return string_iterator(*this); }

    private:
        deque<uint64_t> m_storage;
        deque<uint32_t> m_read_length;
        size_t m_total_seq;
        int m_front_shift;
    };

class CDBGraphDigger {
public:
    typedef list<string> TStrList;
    typedef deque<CDBGraph::Successor> TBases;

    CDBGraphDigger(CDBGraph& graph, double fraction, int jump, int low_count) : m_graph(graph), m_fraction(fraction), m_jump(jump), m_hist_min(graph.HistogramMinimum()), m_low_count(low_count) { 
        m_max_branch = 200;
        cerr << "Valley: " << m_hist_min << endl; 
    }

private:
    typedef tuple<TStrList::iterator,int> TContigEnd;

public:
    void ImproveContigs(TStrList& contigs, int SCAN_WINDOW) { // 0 - no scan
        int kmer_len = m_graph.KmerLen();

        TStrList new_contigs;

        ERASE_ITERATE(TStrList, ic, contigs) {
            if((int)ic->size() < kmer_len+2*SCAN_WINDOW) {

                /* Don't keep shorter contigs
                new_contigs.push_back(*ic);
                */

                contigs.erase(ic);
            }
        }
        
        cerr << "Kmer: " << m_graph.KmerLen() << " Graph size: " << m_graph.GraphSize() << " Contigs in: " << contigs.size() << endl;

        size_t kmers = 0;
        size_t kmers_in_graph = 0;
        //mark already used kmers
        for(string& contig : contigs) {
            CReadHolder rh;
            rh.PushBack(contig);
            for(CReadHolder::kmer_iterator itk = rh.kbegin(kmer_len); itk != rh.kend(); ++itk) {
                ++kmers;
                CDBGraph::Node node = m_graph.GetNode(*itk);
                if(node) {
                    m_graph.SetVisited(node);
                    ++kmers_in_graph;
                }
            }
        }
        cerr << "Kmers in contigs: " << kmers << " Not in graph: " << kmers-kmers_in_graph << endl;

        size_t num = 0;
        for(size_t index = 0; index < m_graph.GraphSize(); ++index) {
            CDBGraph::Node node = 2*(index+1);
            string contig = GetContigForKmer(node, kmer_len);
            if(!contig.empty()) {
                ++num;
                contigs.push_back(contig);                                                    
            }
        }
        cerr << "New seeds: " << num << endl;
        
        //create landing spots
        unordered_map<CDBGraph::Node, TContigEnd> landing_spots;
        landing_spots.reserve(2*(SCAN_WINDOW+1)*contigs.size());
        typedef list<CDBGraph::Node> TNodeList;
        typedef pair<TStrList::iterator, TNodeList> TItL;
        list<TItL> landing_spots_for_contig;
        NON_CONST_ITERATE(TStrList, ic, contigs) {
            string& contig = *ic;
            int len = contig.size();
            landing_spots_for_contig.push_back(make_pair(ic,TNodeList()));
            TNodeList& contig_spots = landing_spots_for_contig.back().second;
            int scan_window = min(SCAN_WINDOW,(len-kmer_len)/2);
            
            CReadHolder lrh;
            string lchunk = contig.substr(0,kmer_len+scan_window);
            lrh.PushBack(lchunk);
            int shift = scan_window+1;
            for(CReadHolder::kmer_iterator itk = lrh.kbegin(kmer_len); itk != lrh.kend(); ++itk) { // gives kmers in reverse order!
                CDBGraph::Node node = m_graph.GetNode(*itk);
                if(node) {
                    landing_spots[node] = TContigEnd(ic, shift);
                    contig_spots.push_back(node);
                }
                --shift;
            }
            CReadHolder rrh;
            string rchunk = contig.substr(contig.size()-kmer_len-scan_window, kmer_len+scan_window);
            ReverseComplement(rchunk.begin(), rchunk.end());
            rrh.PushBack(rchunk);
            shift = scan_window+1;
            for(CReadHolder::kmer_iterator itk = rrh.kbegin(kmer_len); itk != rrh.kend(); ++itk) { // gives kmers in reverse order!
                CDBGraph::Node node = m_graph.GetNode(*itk);
                if(node) {
                    landing_spots[node] = TContigEnd(ic, -shift);
                    contig_spots.push_back(node);
                }
                --shift;
            }                    
        }

        while(!contigs.empty()) {
            TStrList::iterator ic = contigs.begin();
            new_contigs.push_back(*ic);
            string& contig = new_contigs.back();
            list<TStrList::iterator> included;
            included.push_back(contigs.begin());
            list<TItL>::iterator il = find_if(landing_spots_for_contig.begin(),landing_spots_for_contig.end(),[ic](const TItL& a){return a.first == ic;});
            for(CDBGraph::Node ls : il->second)
                landing_spots.erase(ls);
            landing_spots_for_contig.erase(il);

            for(int side = 0; side < 2; ++side) {
                while(true) {
                    int len = contig.size();
                    int scan_window = min(SCAN_WINDOW,(len-kmer_len)/2);
                    string extended_contig = contig;
                    bool connected = false;
                    for(int shift = 0; shift <= scan_window && !connected; ++shift) {
                        TKmer rkmer(contig.end()-kmer_len-shift, contig.end()-shift);
                        CDBGraph::Node rnode = m_graph.GetNode(rkmer);
                        if(!rnode)          // invalid kmer 
                            continue;
                        CDBGraphDigger::TBases step = ExtendToRightAndConnect(rnode, landing_spots);
                        if(step.empty())
                            continue;
                        CDBGraph::Node last_node = step.back().m_node;
                        if(landing_spots.count(last_node)) {                                //connected
                            //update contig 
                            contig = contig.substr(0,len-shift);
                            for(auto& suc : step)
                                contig.push_back(suc.m_nt);
                            TContigEnd cend = landing_spots[last_node];
                            TStrList::iterator ir = get<0>(cend);
                            list<TItL>::iterator il = find_if(landing_spots_for_contig.begin(),landing_spots_for_contig.end(),[ir](const TItL& a){return a.first == ir;});
                            for(CDBGraph::Node ls : il->second)
                                landing_spots.erase(ls);
                            landing_spots_for_contig.erase(il);
                            string rcontig = *ir;
                            int landing_shift = get<1>(cend);
                            if(landing_shift < 0) {
                                ReverseComplement(rcontig.begin(), rcontig.end());
                                landing_shift = -landing_shift;
                            }
                            --landing_shift;;
                            contig += rcontig.substr(kmer_len+landing_shift);
                            included.push_back(ir);
                            connected = true;
                        } else {
                            for(int clip = min((int)step.size(), m_graph.KmerLen()); clip > 0; --clip) {
                                m_graph.ClearVisited(step.back().m_node);
                                step.pop_back();                    
                            }
                            if(contig.size()-shift+step.size() > extended_contig.size()) {  //extended
 	                            extended_contig = contig.substr(0,len-shift);
 	                            for(auto& suc : step)
 	                                extended_contig.push_back(suc.m_nt);                            
                            }
                        }  
                    }
                    if(!connected) {
                        if(extended_contig.size() > contig.size())
                            contig = extended_contig;
                        break;
                    }
                }
                ReverseComplement(contig.begin(),contig.end());
            }            
            
            //clean-up
            for(TStrList::iterator ic : included) {
                contigs.erase(ic);            
            }
        }

        swap(contigs, new_contigs); 

        vector<size_t> contigs_len;
        size_t genome_len = 0;
        for(string& contig : contigs) {
            contigs_len.push_back(contig.size());
            genome_len += contigs_len.back();
        }
        sort(contigs_len.begin(), contigs_len.end());
        size_t n50 = 0;
        int l50 = 0;
        size_t len = 0;
        for(int j = (int)contigs_len.size()-1; j >= 0 && len < 0.5*genome_len; --j) {
            ++l50;
            n50 = contigs_len[j];
            len += contigs_len[j];
        }
        cerr << "Contigs out: " << contigs_len.size() << " Genome: " << genome_len << " N50: " << n50 << " L50: " << l50 << endl;           
    }

    //assembles the contig; changes the state of all used nodes to 'visited'
    string GetContigForKmer(const CDBGraph::Node& initial_node, int clip) {
        if(m_graph.IsVisited(initial_node) || m_graph.Abundance(initial_node) < m_hist_min || !GoodNode(initial_node))
            return string();

        unordered_map<CDBGraph::Node, TContigEnd> landing_spots;
        TBases contigr = ExtendToRightAndConnect(initial_node, landing_spots);
        TBases contigl = ExtendToRightAndConnect(CDBGraph::ReverseComplement(initial_node), landing_spots);
                      
        int contigl_size = contigl.size();
        int contigr_size = contigr.size();
        int kmer_len = m_graph.KmerLen();
        int total_len = contigl_size+kmer_len+contigr_size;

        
        if(total_len < kmer_len+2*clip) {
            m_graph.ClearVisited(initial_node);
            for(auto& base : contigl)
                m_graph.ClearVisited(base.m_node);
            for(auto& base : contigr)
                m_graph.ClearVisited(base.m_node);

            return string();
        }
        

        string contig;
        int pos = 0;
        for(int j = contigl_size-1; j >= 0; --j, ++pos) {
            if(pos >= clip && pos < total_len-clip)
                contig.push_back(Complement(contigl[j].m_nt));
            else
                m_graph.ClearVisited(contigl[j].m_node);
        }
        string kmer = m_graph.GetNodeSeq(initial_node);
        for(int j = 0; j < kmer_len; ++j, ++pos) {
            if(pos >= clip && pos < total_len-clip)
                contig.push_back(kmer[j]);
        }
        for(int j = 0; j < contigr_size; ++j, ++pos) {
            if(pos >= clip && pos < total_len-clip)
                contig.push_back(contigr[j].m_nt);
            else
               m_graph.ClearVisited(contigr[j].m_node); 
        }
        if(min(contigl_size,contigr_size) < clip+1)
            m_graph.ClearVisited(initial_node);

        return contig;               
    }

    string MostLikelyExtension(CDBGraph::Node node, unsigned len) const { //don't do FilterNeighbors because it is called in it
        string s;
        while(s.size() < len) {
            vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(node);
            if(successors.empty())
                return s;            
            sort(successors.begin(), successors.end(), [&](const CDBGraph::Successor& a, const CDBGraph::Successor& b) {return m_graph.Abundance(a.m_node) > m_graph.Abundance(b.m_node);}); 
            node = successors[0].m_node;
            s.push_back(successors[0].m_nt);
        }
        return s;
    }            

    string MostLikelySeq(CDBGraph::Successor base, unsigned len) const {
        string s(1, base.m_nt);
        return s+MostLikelyExtension(base.m_node, len-1);
    }

    string StringentExtension(CDBGraph::Node node, unsigned len) const {
        string s;
        while(s.size() < len) {
            vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(node);
            FilterNeighbors(successors);
            if(successors.size() != 1)
                return s;            
            node = successors[0].m_node;
            s.push_back(successors[0].m_nt);
        }
        return s;
    }


    bool GoodNode(const CDBGraph::Node& node) const { return m_graph.Abundance(node) > m_low_count; }

    void FilterNeighbors(vector<CDBGraph::Successor>& successors) const {
        if(successors.size() > 1) {
            int abundance = 0;
            for(auto& suc : successors) {
                abundance += m_graph.Abundance(suc.m_node);
            }
            sort(successors.begin(), successors.end(), [&](const CDBGraph::Successor& a, const CDBGraph::Successor& b) {return m_graph.Abundance(a.m_node) > m_graph.Abundance(b.m_node);});
            for(int j = successors.size()-1; j > 0 && m_graph.Abundance(successors.back().m_node) <= m_fraction*abundance; --j) 
                successors.pop_back();            
        }
        
        if(m_graph.GraphIsStranded() && successors.size() > 1) {

            double fraction = 0.1*m_fraction;
            
            int target = -1;
            for(int j = 0; target < 0 && j < (int)successors.size(); ++j) {
                if(m_graph.GetNodeSeq(successors[j].m_node).substr(m_graph.KmerLen()-3) == "GGT") 
                    target = j;
            }
            if(target >= 0 && GoodNode(successors[target].m_node)) {
                double am = m_graph.Abundance(successors[target].m_node)*(1-m_graph.PlusFraction(successors[target].m_node));
                for(int j = 0; j < (int)successors.size(); ) {
                    if(m_graph.Abundance(successors[j].m_node)*(1-m_graph.PlusFraction(successors[j].m_node)) < fraction*am)
                        successors.erase(successors.begin()+j);
                    else
                        ++j;
                }
                return;
            }

            for(int j = 0; target < 0 && j < (int)successors.size(); ++j) {
                if(MostLikelySeq(successors[j], 3) == "ACC") 
                    target = j;
            }
            if(target >= 0 && GoodNode(successors[target].m_node)) {
                double ap = m_graph.Abundance(successors[target].m_node)*m_graph.PlusFraction(successors[target].m_node);
                for(int j = 0; j < (int)successors.size(); ) {
                    if(m_graph.Abundance(successors[j].m_node)*m_graph.PlusFraction(successors[j].m_node) < fraction*ap)
                        successors.erase(successors.begin()+j);
                    else
                        ++j;
                }
                return;
            }
                       
            bool has_both = false;
            for(int j = 0; !has_both && j < (int)successors.size(); ++j) {
                double plusf = m_graph.PlusFraction(successors[j].m_node);
                double minusf = 1.- plusf;
                has_both = GoodNode(successors[j].m_node) && (min(plusf,minusf) > 0.25);
            }

            if(has_both) {
                for(int j = 0; j < (int)successors.size(); ) {
                    double plusf = m_graph.PlusFraction(successors[j].m_node);
                    double minusf = 1.- plusf;
                    if(min(plusf,minusf) < fraction*max(plusf,minusf))
                        successors.erase(successors.begin()+j);
                    else
                        ++j;
                }
            }                      
        }
    }

    enum EConnectionStatus {eSuccess, eNoConnection, eAmbiguousConnection};

    struct SElement {
        SElement (CDBGraph::Successor suc, SElement* link) : m_link(link), m_suc(suc) {}
        struct SElement* m_link;   // previous element
        CDBGraph::Successor m_suc;
    };

    pair<TBases, EConnectionStatus> ConnectTwoNodes(const CDBGraph::Node& first_node, const CDBGraph::Node& last_node, int steps) const {
        
        pair<TBases, EConnectionStatus> bases(TBases(), eNoConnection);

        deque<SElement> storage; // will contain ALL extensions (nothing is deleted)
        typedef unordered_map<CDBGraph::Node,SElement*> TElementMap;  //pointer to its own element OR zero if umbiguous path
        TElementMap current_elements;

        vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(first_node);
        FilterNeighbors(successors);
        for(auto& suc : successors) {
            storage.push_back(SElement(suc, 0));
            current_elements[suc.m_node] = &storage.back();
        }

        list<SElement> connections;
        for(int step = 1; step < steps && !current_elements.empty(); ++step) {
            TElementMap new_elements;
            for(auto& el : current_elements) {
                vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(el.first);
                FilterNeighbors(successors);
                if(el.second == 0) {  // umbiguous path
                    for(auto& suc : successors) {
                        new_elements[suc.m_node] = 0;
                        if(suc.m_node == last_node) {
                            bases.second = eAmbiguousConnection;
                            return bases;
                        }
                    }
                } else {
                    for(auto& suc : successors) {
                        storage.push_back(SElement(suc, el.second));
                        if(suc.m_node == last_node) {
                            if(!connections.empty()) {
                                bases.second = eAmbiguousConnection;
                                return bases;
                            } else {
                                connections.push_back(storage.back()); 
                            }                           
                        }
                        
                        pair<TElementMap::iterator, bool> rslt = new_elements.insert(make_pair(suc.m_node, &storage.back()));
                        if(!rslt.second || !GoodNode(suc.m_node))
                            rslt.first->second = 0;
                        
                    }
                }                    
            }

            swap(current_elements, new_elements);
            if(current_elements.size() > m_max_branch)
                return bases;
        }

        if(connections.empty())
            return bases;

        SElement el = connections.front();
        while(el.m_link != 0) {
            bases.first.push_front(el.m_suc);
            el = *el.m_link;
        }
        bases.first.push_front(el.m_suc);
        bases.second = eSuccess;
        return bases;
    }

private:

    typedef tuple<TBases,size_t> TSequence;
    typedef list<TSequence> TSeqList;
    typedef unordered_map<CDBGraph::Node, tuple<TSeqList::iterator, bool>> TBranch;  // all 'leaves' will have the same length

    bool OneStepBranchExtend(TBranch& branch, TSeqList& sequences) {
        TBranch new_branch;
        for(auto& leaf : branch) {
            vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(leaf.first);
            FilterNeighbors(successors);
            if(successors.empty()) {
                sequences.erase(get<0>(leaf.second));
                continue;
            }
            for(int i = successors.size()-1; i >= 0; --i) {
                TSeqList::iterator is = get<0>(leaf.second);
                if(i > 0) {  // copy sequence if it is a fork           
                    sequences.push_front(*is);
                    is = sequences.begin();
                }
                TBases& bases = get<0>(*is);
                size_t& abundance = get<1>(*is);
                bases.push_back(successors[i]);
                CDBGraph::Node& node = successors[i].m_node;
                abundance += m_graph.Abundance(node);

                pair<TBranch::iterator, bool> rslt = new_branch.insert(make_pair(node, make_tuple(is, false)));
                if(!rslt.second) {  // we alredy have this kmer - select larger abundance or fail
                    get<1>(rslt.first->second) = true;
                    TSeqList::iterator& js = get<0>(rslt.first->second);    // existing one 
                    if(abundance > get<1>(*js)) {                   // new is better    
                        //if(m_fraction*abundance > get<1>(*js)) {                   // new is better  
                        sequences.erase(js);
                        js = is;
                    } else if(abundance < get<1>(*js)) {            // existing is better   
                        //} else if(abundance < m_fraction*get<1>(*js)) {            // existing is better 
                        sequences.erase(is);                                        
                    } else {                                        // equal 
                        branch.clear();
                        return false;
                    }
                }
            }
        }

        swap(branch, new_branch);
        return true;
    }

    TBases JumpOver(vector<CDBGraph::Successor>& successors, int max_extent, int min_extent) {
        //if(max_extent == 0 || m_hist_min == 0)
        if(max_extent == 0)
            return TBases();

        TBranch extensions;
        TSeqList sequences; // storage
        for(auto& suc : successors) {
            sequences.push_front(TSequence(TBases(1,suc), m_graph.Abundance(suc.m_node)));
            extensions[suc.m_node] = make_tuple(sequences.begin(), false);         
        }

        while(!extensions.empty() && extensions.size() < m_max_branch) {
            TSeqList::iterator is = get<0>(extensions.begin()->second);
            int len = get<0>(*is).size();
            if(len == max_extent)
                break;
            
            if(!OneStepBranchExtend(extensions, sequences) || extensions.empty())  // ambiguos buble in a or can't extend
                return TBases();

            if(extensions.size() == 1 && len+1 >= min_extent)
                break;
        }

        if(extensions.size() == 1 && !get<1>(extensions.begin()->second)) {
            TSeqList::iterator is = get<0>(extensions.begin()->second);
            TBases& bases = get<0>(*is);
            bool all_good = true;
            for(auto& base : bases) {
                if(!GoodNode(base.m_node)) {
                    all_good = false;
                    break;
                }
            }
            if(bases.front().m_nt == successors[0].m_nt && all_good) // the variant with higher abundance survived
                return bases;            
        }

        return TBases();        
    }

    template<typename T>  // T has to have fast count(elem)
    TBases ExtendToRightAndConnect(const CDBGraph::Node& initial_node, const T& landing_spots) {
        CDBGraph::Node node = initial_node;
        TBases extension;
        int max_extent = m_jump;
        m_graph.SetVisited(node);
        while(true) {
            int extension_size = extension.size();
            vector<CDBGraph::Successor> successors = m_graph.GetNodeSuccessors(node);
            FilterNeighbors(successors);
            if(successors.empty())                    // no extensions
                break;             

            TBases step;
            if(successors.size() > 1) {                // test for dead end   
                step = JumpOver(successors, max_extent, 0);
            } else {                                   // simple extension  
                step.push_back(successors.front());
            }
            if(step.empty())                    // multiple extensions
                break;

            bool all_good = true;
            for(auto& s : step) {
                if(!GoodNode(s.m_node)) {
                    all_good = false;
                    break;
                }
            }
            if(!all_good)
                break;

            int step_size = step.size();
        
            CDBGraph::Node rev_node = CDBGraph::ReverseComplement(step.back().m_node);
            vector<CDBGraph::Successor> predecessors = m_graph.GetNodeSuccessors(rev_node);
            FilterNeighbors(predecessors);
            if(predecessors.empty())                     // no extensions
                break; 

            TBases step_back;
            if(predecessors.size() > 1 || step_size > 1)
                step_back = JumpOver(predecessors, max_extent, step_size);
            else
                step_back.push_back(predecessors.front());

            int step_back_size = step_back.size();
            if(step_back_size < step_size)
                break;

            bool good = true;
            for(int i = 0; i <= step_size-2 && good; ++i)
                good = (CDBGraph::ReverseComplement(step_back[i].m_node) == step[step_size-2-i].m_node);
            for(int i = step_size-1; i < min(step_back_size, extension_size-1+step_size) && good; ++i)
                good = (CDBGraph::ReverseComplement(step_back[i].m_node) == extension[extension_size-1+step_size-1-i].m_node);
            if(!good)
                break;
       
            int overshoot = step_back_size-step_size-extension_size;
            if(overshoot >= 0 && CDBGraph::ReverseComplement(step_back[step_back_size-1-overshoot].m_node) != initial_node) 
                break;

            if(overshoot > 0) { // overshoot
                CDBGraph::Node over_node = CDBGraph::ReverseComplement(step_back.back().m_node);
                vector<CDBGraph::Successor> oversuc = m_graph.GetNodeSuccessors(over_node);
                FilterNeighbors(oversuc);
                if(oversuc.empty())
                    break;
                TBases step_over;
                if(oversuc.size() > 1 || overshoot > 1)
                    step_over = JumpOver(oversuc, max_extent, overshoot);
                else
                    step_over.push_back(oversuc.front());

                if((int)step_over.size() < overshoot)
                    break;
                for(int i = 0; i < overshoot && good; ++i)
                    good = (CDBGraph::ReverseComplement(step_over[i].m_node) == step_back[step_back_size-2-i].m_node);
                if(!good)
                    break;
            }

            bool repeat = false;
            for(auto& s : step) {
                extension.push_back(s);
                if(landing_spots.count(s.m_node)) {      // found connection
                    return extension;
                } else if(m_graph.IsVisited(s.m_node)) { // repeat 
                    extension.pop_back();
                    repeat = true;
                    break;
                }
                m_graph.SetVisited(s.m_node);
            }
            if(repeat)
                break;

            node = extension.back().m_node;
        }
        
        return extension;
    }


    CDBGraph& m_graph;
    double m_fraction;
    int m_jump;
    int m_hist_min;
    int m_low_count;
    size_t m_max_branch;
};

}; // namespace
#endif /* _DeBruijn_Graph_ */
