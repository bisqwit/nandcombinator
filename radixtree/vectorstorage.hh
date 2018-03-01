#include "storage.hh"
#include "templates.hh"

#include <cstring>

/* VectorStorage is storage of vectors.
 * Designed for optimal storage of RadixTree nodes.
 * As a backend, uses MemoryStorage<> objects.
 *
 * Public properties:

    // A type that looks approximately like std::vector<unsigned char>
    class VecType
    {
    public:
        const unsigned char operator[] (size_t ind) const;
        void assign(const unsigned char* begin, const unsigned char* end);
        bool empty() const;
        size_t size() const;
        void clear();
        void replace(size_t beginpos, size_t orig_size,
                     const unsigned char* source, size_t new_size);
        const unsigned char* GetReadPtr(size_t pos, size_t length) const;
    private:
        // private properties omitted
    };

    typedef size_t IDtype;

    // Allocate a new vector.
    // Return value: ID that can be used to access the vector.
    static IDtype Create();

    // Wipe out everything
    static void clear();

    // Deallocates the given vector
    static void Free(IDtype id);

    // Read(): Returns a reference to the vector by given number.
    //
    // References returned by this call are guaranteed to be
    // valid only until the next call of Read(), Create(), Free() or clear().
    //
    // This function always returns a reference to VectorStorage::Buf.
    static VecType& Read(IDtype n);

    // The storage buffer into which Read() returns a reference.
    // Read() also initializes it.
    static VecType Buf;

*/



class VectorStorage
{
private:
    typedef unsigned short LengthType;
    typedef MemoryStorage<unsigned char> DataStorageType;

public:
    class VecType
    {
    private:
        unsigned char* Buf;
        LengthType Length;
        bool owned, changed;
    private:
        /* No copying. */
        VecType(const VecType&);
        void operator=(const VecType&);
    public:
        VecType(): Buf(0),Length(0),owned(false),changed(false) { }
        ~VecType() { if(owned && Buf) std::free(Buf); }

        inline bool was_changed() const { return changed; }
        inline void reset_change() { changed=false; }

        inline unsigned char operator[] (size_t ind) const
        {
            return Buf[ind];
        }
        inline void initial_assign_readonly
            (const unsigned char* begin, const unsigned char* end)
        {
            if(owned && Buf) std::free(Buf);
            Buf    = const_cast<unsigned char*>(begin);
            Length = end-begin;
            owned  = false;
            changed= false;
        }
        void assign(const unsigned char* begin, const unsigned char* end)
        {
            size_t NewLen = end-begin;
            if(!owned) { Buf = (unsigned char*)std::malloc(Length = NewLen); owned=true; }
            if(Length != NewLen)
            {
                if(Buf) std::free(Buf);
                Buf = (unsigned char*) std::malloc(Length = NewLen);
            }
            std::memcpy(Buf, begin, Length);
            changed=true;
        }
        inline bool empty() const { return Length == 0; }
        inline size_t size() const { return Length; }
        inline void clear()
        {
            if(owned && Buf) std::free(Buf);
            Buf=0;
            Length=0;
            owned=false;
            changed=true;
        }
        void replace(size_t beginpos, size_t orig_size,
                     const unsigned char* source, size_t new_size);
        inline const unsigned char* GetReadPtr(size_t pos, size_t length) const
        {
            if(!length) return (const unsigned char*)"";
            return Buf+pos;
        }
    };

public:
    /* The storage introduced by Read().
     * It acts like a vector, and it masquerades as the
     * actual storage of the SimpleVec item, but in fact
     * it's just a relay.
     */
    static VecType Buf;

private:
    /* The storage where all data is stored in.
     * It is governed by the StupidVec items and clear().
     */
    static DataStorageType storage;

private:
    class StupidVec
    {
        size_t Ptr;
        LengthType Length, AllocLength;
    public:
        StupidVec() : Ptr(0),Length(0),AllocLength(0) { }
        /* Warning: destructor is never called. */
        
        void Reassign(const VecType& vec);
        const unsigned char* GetBuf() const
        {
            return storage.GetReadPtr(Ptr, Length);
        }
        size_t GetLength() const { return Length; }
    } __attribute__((packed));
    
public:
    typedef size_t IDtype;
private:
    static IDtype BufOwner;
    static IDtype ID;
    static size_t used;
    
    /* The storage of pointers to StupidVec items.
     * Indexed by the "IDs" which are the public
     * interface to this allocator.
     * It is governed by Create(), Free1() and clear().
     */
    typedef MemoryStorage<StupidVec> PointerStorageType;
    static PointerStorageType ptrs;
public:
    /* Allocate a new vector */
    static IDtype Create();
    
    /* Wipe out everything */
    static void clear();
    
    /* Deallocates the given vector */
    static void Free(IDtype id);
    
    /* Returns a reference to the vector by given number.
     *
     * References returned by this call are guaranteed to be
     * valid only until the next call of Read(), Create(), Free() or clear().
     *
     * This function always returns a reference to VectorStorage::Buf.
     */
    static VecType& Read(IDtype n);

private:
    /* Instances of this class are not to be created. */
    VectorStorage();
    VectorStorage(const VectorStorage&);
};
