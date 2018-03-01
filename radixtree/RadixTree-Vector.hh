#include "endian.hh" /* For Rn, Wn */
#include "vectorstorage.hh"
#include "templates.hh"

#undef bytecode
#define bytecode VectorStorage::Buf

#include <array>

/*

    Structure:

    Each node of the radix tree has an address.
    The address is called an "ID", and it is a key to VectorStorage.
    Using the ID, the node can be retrieved.

    The structure of the node, called "bytecode", is as follows:
      byte 0:
        unsigned char IDsize; // size of IDs in this table
      remain: array[] of
        TokenType Byte;
          // the array value to be searched for

        If leaf-type node:
          unsigned char payload[];
        else:
          size_t relative_child_ID : (IDsize * 8);
          // the child ID; stored using IDsize bytes

      The array is sorted, so as to enable a lower_bound() search.

     To convert the child ID into an absolute ID, the following formula is used:
       absolute_child_ID = current_ID + 1 + relative_child_ID

     The Leaf stores the actual data of the node.
*/


static inline unsigned CalcLinkBytes(size_t LinkValue)
{
    return CalcBytesNeededByInt(LinkValue);
}



template<size_t RealSize, size_t PayloadSize=0, unsigned TokenBits = 8>
struct RadixTree
{
    enum { TokenSize = (TokenBits + 7) / 8 };
    typedef std::array<unsigned char, RealSize> KeyType;
private:
    typedef RadixTree<RealSize,PayloadSize,TokenBits> me;

public://protected:
    inline size_t size() const { return Size; }
    void clear()
    {
        DeleteRoot();
        Size=0;
        RootID = VectorStorage::Create();
    }

    RadixTree(): Size(0), RootID(VectorStorage::Create())
    {
    }
    ~RadixTree()
    {
        DeleteRoot();
    }

    // If Key is not found, returns false.
    // If Key is found, Data is filled, returns true.
    bool find(const KeyType& Key, unsigned char* Data = 0) const
    {
        return const_cast<me*>(this)->add_or_find_or_update<false, false,true> (Key,Data);
    }

    // If Key is not found, adds Key & Data.
    // If Key is found, updates the saved Data.
    void ensure(const KeyType& Key, const unsigned char* Data)
    {
        add_or_find_or_update<true, true,false> (Key, const_cast<unsigned char*> (Data));
    }

    // If Key is not found, adds Key & Data, returns true.
    // If Key is found, returns false.
    bool add(const KeyType& Key, const unsigned char* Data = 0)
    {
        return ! add_or_find_or_update<true, false,false> (Key, const_cast<unsigned char*> (Data));
    }

    // If Key is not found, adds Key & Data, returns true.
    // If Key is found, Data is filled, returns false.
    bool add_or_find(const KeyType& Key, unsigned char* Data)
    {
        return ! add_or_find_or_update<true, false,true> (Key,Data);
    }

private:
    /* Returns false if not found, true if found */
    template<bool addp, bool update_struct, bool update_paramdata>
    bool add_or_find_or_update(const KeyType& Key, unsigned char* Data)
    {
        //if(RealSize == 3 && TokenBits == 12)
        //    DumpKey(Key, "Finding ", Data);

        void* p = Process<addp>(Key, Data);
        if(!p) return false; // not found, maybe added.

        if(PayloadSize)
        {
            if(update_struct)
            {
                std::memcpy(p, Data, PayloadSize);
            }
            if(update_paramdata)
            {
                std::memcpy(Data, p, PayloadSize);
            }
        }
        return true;
    }

private:
    RadixTree(const me&);
    void operator=(const me&);

    template<bool addp>
    void* ProcessHelper
        (const KeyType& Key,
         VectorStorage::IDtype Cur,
         size_t level, size_t offset,
         unsigned node_size,
         const unsigned char* data
        )
    {
        const unsigned want = Key[level];

        const size_t remain_bytes = bytecode.size()-offset;

        const unsigned char
            *begin = bytecode.GetReadPtr(offset, remain_bytes),
            *end   = begin + remain_bytes,
            *middle;

        size_t len = remain_bytes / node_size;
        //fprintf(stderr, "(%ld)remain=%u,size=%u\n", (long)Cur, (unsigned)remain_bytes,(unsigned)node_size);

        /* use lower_bound to search for the byte */
        const unsigned char* focus = begin;
        while(len > 0)
        {
            size_t half   = len/2;
            middle = focus + half * node_size;
            if(Rn(middle, TokenSize) < want)
            {
                focus = middle + node_size;
                len = len - half - 1;
            }
            else
                len = half;
        }
        if(focus == end || Rn(focus, TokenSize) != want)
        {
            //fprintf(stderr, "(%ld)not found @ %u(%02X)\n", (long)Cur, (unsigned)level, want);
            if(addp)
            {
                /* Pass this address to AddBack. */
                const size_t bytepos = focus - begin; // note: ignores id_width
                AddBack(Key, Cur,bytepos, level, data);
            }
            return 0;
        }
        return (void*) (focus + TokenSize);
    }

    /* Does a search on KeySeq. */
    template<bool addp>
    void* Process(const KeyType& Key,
                  const unsigned char* data)
    {
        size_t level = 0;

        VectorStorage::IDtype Cur = RootID;
        for(; level+1 < RealSize; ++level)
        {
            VectorStorage::Read(Cur);

            if(bytecode.empty())
            {
                if(addp) AddBack(Key, Cur,0, level, data);
                return 0;
            }

            /* Note: The "offset" must be "1" here, to skip over id_width. */
            const unsigned id_width = bytecode[0];

            void* p = ProcessHelper<addp>(Key, Cur,level, 1, TokenSize+id_width, data);
            if(!p) return 0;

            Cur += 1 + Rn(p, id_width);
        }
        if(RealSize == 0) return 0;

        VectorStorage::Read(Cur);
        return ProcessHelper<addp> (Key, Cur,level, 0, TokenSize+PayloadSize, data);
    }

private:
    size_t Size;
    VectorStorage::IDtype RootID;

    inline void DeleteRoot()
    {
        DeleteArray(RootID, 0);
    }
    inline void DeleteArray(VectorStorage::IDtype id, size_t level)
    {
        if(level+1 < RealSize)
        {
            VectorStorage::Read(id);

            if(!bytecode.empty())
            {
                const unsigned id_width = bytecode[0];
                const size_t remain_bytes = bytecode.size() - 1;
                const size_t remain_nodes = remain_bytes / (TokenSize + id_width);

                for(size_t a=0; a<remain_nodes; ++a)
                {
                    const unsigned char* p =
                        bytecode.GetReadPtr(
                            1 + a * (TokenSize + id_width) + TokenSize,
                            id_width);

                    DeleteArray(id + 1 + Rn(p, id_width), level + 1);

                    if(a+1 < remain_nodes) VectorStorage::Read(id); // reread the node
                }
            }
        }
        else
        {
            /* leafs don't need special handling. */
        }
        VectorStorage::Free(id);
    }

    void AddBack(const KeyType& Key,
                 VectorStorage::IDtype id, size_t bytepos, size_t level,
                 const unsigned char* Data)
    {
        if(level+1 >= RealSize) /* leaf? */
        {
            unsigned char node[PayloadSize + TokenSize];

            Wn(node, Key[level], TokenSize);

            if(PayloadSize)
                std::memcpy(node + TokenSize, Data, PayloadSize);

            bytecode.replace(bytepos, 0, node, PayloadSize + TokenSize);

#if 0
            fprintf(stderr, "(%ld)wrote tail %02X: ", (long)id, Key[level]);
            for(unsigned a=0; a<PayloadSize+TokenSize; ++a)
                fprintf(stderr, "%02X ", node[a]);
            fprintf(stderr, " -> %u\n", (unsigned)bytecode.size());
#endif

            ++Size;
            DumpKey(Key, "Added ", Data);
            Dump(Key, "After adding something");
            return;
        }

        /* The intermediate nodes */
        VectorStorage::IDtype NewID = VectorStorage::Create();
        const size_t Delta = NewID-id-1;

        unsigned nbytes = CalcLinkBytes(Delta);
        if(bytecode.empty())
        {
            /* Easy! We get to choose our own data size */
            unsigned char node[1 + TokenSize + nbytes];

            node[0] = nbytes;
            Wn(1+node, Key[level], TokenSize);
            Wn(1+node+TokenSize, Delta, nbytes);

#if 0
            fprintf(stderr, "(%ld)wrote plain %02X(%d): ", (long)id, Key[level], nbytes);
            for(unsigned a=0; a<1+TokenSize+nbytes; ++a)
                fprintf(stderr, "%02X ", node[a]);
            fprintf(stderr, " -> %u\n", (unsigned)bytecode.size());
#endif

            bytecode.replace(bytepos, 0, node, 1 + TokenSize + nbytes);
        }
        else
        {
            unsigned char old_nbytes = bytecode[0];
            if(old_nbytes >= nbytes)
            {
                /* Pick the existing size */
                nbytes = old_nbytes;

                unsigned char node[TokenSize + 16 + nbytes];

                Wn(node, Key[level], TokenSize);
                Wn(node+TokenSize, Delta, nbytes);

                bytecode.replace(bytepos+1, 0, node, TokenSize + nbytes);

#if 0
                fprintf(stderr, "(%ld)added plain %02X(%d): ", (long)id, Key[level], nbytes);
                for(unsigned a=0; a<TokenSize+nbytes; ++a)
                    fprintf(stderr, "%02X ", node[a]);
                fprintf(stderr, " -> %u\n", (unsigned)bytecode.size());
#endif
            }
            else
            {
                size_t old_size = TokenSize + old_nbytes;
                size_t new_size = TokenSize + nbytes;

                /* Need to resize the existing data! */
                size_t Size = (bytecode.size()-1) / old_size;
                const size_t NewBufSize = 1 + (Size+1) * new_size;
                unsigned char NewBuf[NewBufSize];

                const size_t InsertIndex = bytepos / old_size;
                NewBuf[0] = nbytes;

                /* While resizing the existing data, we also poke the
                 * new entry into the structure. Saves some work.
                 */

                const unsigned char* SrcData = bytecode.GetReadPtr(1, bytecode.size()-1);

                for(size_t s=0, d=0; s<=Size; ++s, ++d)
                {
                    unsigned char*      dest = &NewBuf[1  + d*new_size];
                    const unsigned char* src = &SrcData[0 + s*old_size];

                    if(d == InsertIndex)
                    {
                        Wn(dest, Key[level], TokenSize);
                        Wn(dest+TokenSize, Delta, nbytes);
                        ++d;
                        dest += new_size;
                    }

                    if(s >= Size) break;

                    Wn(dest, Rn(src, TokenSize), TokenSize);
                    Wn(dest+TokenSize, Rn(src+TokenSize, old_nbytes), nbytes);
                }

                /*
                if(nbytes >= 3)
                std::cerr << "RadixTree-Vector Resize from "
                          << (size_t)old_nbytes << " to " << (size_t)nbytes << "\n";
                */
                bytecode.replace(0,bytecode.size(), NewBuf, NewBufSize);
            }
        }
        VectorStorage::Read(NewID);
        AddBack(Key, NewID, 0, level+1, Data);
    }

private:
    static void DumpKey(const KeyType& /*Key*/, const char* /*msg*/, const unsigned char* /*Data*/)
    {
#if 0
        //if(RealSize != 3 || TokenBits != 12) return;
        fprintf(stderr, "%s ", msg);
        for(size_t a=0; a<RealSize; ++a) fprintf(stderr, "%02X ", Key[a]);
        if(Data)
        {
            for(size_t a=0; a<PayloadSize; ++a) fprintf(stderr, "|%02X", Data[a]);
        }
        fprintf(stderr, "\n");
        std::fflush(stderr);
#endif
    }
    void Dump(const KeyType& /*Key*/, const char* /*message*/)
    {
#if 0
        //if(RealSize != 3 || TokenBits != 12) return;
        fprintf(stderr, "%s:\n", message);
        fprintf(stderr, "Key:");
        for(size_t a=0; a<RealSize; ++a)
            fprintf(stderr, " %02X", Key[a]);
        fprintf(stderr, "\n");

        Dump(RootID, 2, 0);
#endif
    }
    void Dump(VectorStorage::IDtype id, unsigned indent, size_t level)
    {
#if 0
        //if(RealSize != 3 || TokenBits != 12) return;
        if(level+1 < RealSize)
        {
            VectorStorage::Read(id);

            if(bytecode.empty())
            {
                fprintf(stderr, "%*sEmpty node\n", indent,"");
            }
            else
            {
                const unsigned id_width = bytecode[0];
                fprintf(stderr, "%*s[%02X]\n", indent,"", id_width);

                const size_t remain_bytes = bytecode.size() - 1;
                const size_t remain_nodes = remain_bytes / (TokenSize + id_width);

                for(size_t a=0; a<remain_nodes; ++a)
                {
                    const unsigned char* ptr =
                        bytecode.GetReadPtr(
                            1 + a * (TokenSize + id_width),
                            id_width);

                    fprintf(stderr, "%*s%02X: %d\n", indent,"",
                        (unsigned)Rn(ptr, TokenSize),
                        (int)Rn(ptr+TokenSize, id_width));
                    size_t sub_id = id + 1 + Rn(ptr + TokenSize, id_width);
                    Dump(sub_id, indent+1, level+1);
                    if(a+1 < remain_nodes) VectorStorage::Read(id); // reread the node
                }
            }
        }
        else
        {
            VectorStorage::Read(id);

            const size_t nodesize = TokenSize + PayloadSize;

            size_t b = bytecode.size() / nodesize;
            for(size_t a=0; a<b; ++a)
            {
                const unsigned char* ptr = bytecode.GetReadPtr(a*nodesize,0);
                fprintf(stderr, "%*s%02X ", indent,"", (unsigned)Rn(ptr, TokenSize));
                for(size_t c=0; c<PayloadSize; ++c)
                    fprintf(stderr, "|%02X", (ptr+TokenSize)[c]);
                fprintf(stderr, "\n");
            }
        }
#endif
    }
};

#undef bytecode
