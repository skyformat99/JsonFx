
#ifndef _JSONFX_ALLOCATOR_H_
#define _JSONFX_ALLOCATOR_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "JsonFx/Config.h"
#include "JsonFx/internal/Utils.h"
#include "JsonFx/CharSet.h"
#include "JsonFx/Value.h"

#include "jimi/basic/archdef.h"
#include "jimi/basic/stddef.h"
#include "jimi/basic/assert.h"

/* Whether use profile codes when use JsonFx allocators? */
#define JSONFX_ALLOCATOR_USE_PROFILE    1

/* If it is 0, don't allow malloc size more than the default chunk size. */
#define JSONFX_ALLOW_ALLOC_BIGSIZE      0

/* Default malloc alignment size. */
#define JSONFX_MALLOC_ALIGNMENT_SIZE    8

namespace JsonFx {

/* Just define for inheritance. */
class StackAllocator {};

/* Just define for inheritance. */
class PoolAllocator {};

class CrtAllocator : public StackAllocator
{
public:
    static const bool kNeedFree = true;

    static void * malloc(size_t size)  { return std::malloc(size); }
    static void * realloc(void * ptr, size_t size, size_t new_size) {
        (void)size;
        return std::realloc(ptr, new_size);
    }
    static void free(void * ptr) { std::free(ptr); }
    static void free(void * ptr, size_t size) { (void)size; std::free(ptr); }
};

template <size_t ChunkCapacity = kDefaultChunkCapacity,
          typename Allocator = DefaultAllocator>
class StackPoolAllocator : public PoolAllocator
{
public:
    typedef Allocator AllocatorType;

    static const bool kNeedFree = false;

    StackPoolAllocator()  {}
    ~StackPoolAllocator() {}

    void destroy() { /* Do nothing! */ }

    void * allocate(size_t size) { return AllocatorType::malloc(size); }

    void * reallocate(void * ptr, size_t size, size_t new_size) {
        return AllocatorType::realloc(ptr, size, new_size);
    }

    static void deallocate(void * ptr) { return AllocatorType::free(ptr); }
    static void deallocate(void * ptr, size_t size) { return AllocatorType::free(ptr, size); }
};

template <size_t ChunkCapacity = kDefaultChunkCapacity,
          typename Allocator = DefaultAllocator>
class MemoryPoolAllocator : public PoolAllocator
{
public:
    typedef Allocator   AllocatorType;

    static const bool   kNeedFree       = false;
    static const bool   kAutoRelease    = true;
    static const size_t kChunkCapacity  = ChunkCapacity;

    struct ChunkHead {
        void *      cursor;
        size_t      remain;
        size_t      capacity;
        ChunkHead * next;
    };

public:
    MemoryPoolAllocator() : mChunkHead(NULL) {
        init();
    }
    ~MemoryPoolAllocator() {
        destroy();
    }

    void destroy() {
        if (this->kAutoRelease) {
            ChunkHead * pChunkHead = mChunkHead;
            while (pChunkHead != NULL) {
                ChunkHead * next = pChunkHead->next;
                AllocatorType::free(pChunkHead);
                pChunkHead = next;
            }
            mChunkHead = NULL;
        }
    }

private:
    //! Copy constructor is not permitted.
    MemoryPoolAllocator(const MemoryPoolAllocator & rhs);               /* = delete */
    //! Copy assignment operator is not permitted.
    MemoryPoolAllocator& operator =(const MemoryPoolAllocator & rhs);   /* = delete */

    void init() {
        addNewChunk(0);
    }

    void * addNewChunk(size_t size) {
        ChunkHead * newChunk = reinterpret_cast<ChunkHead *>(AllocatorType::malloc(kChunkCapacity));
        jimi_assert(newChunk != NULL);

        void * cursor = reinterpret_cast<void *>(newChunk + 1);

        // Do not handle out-of-memory explicitly.
        newChunk->cursor    = reinterpret_cast<void *>(reinterpret_cast<char *>(cursor) + size);
        newChunk->remain    = kChunkCapacity - (sizeof(ChunkHead) + size);
        newChunk->capacity  = kChunkCapacity;
        newChunk->next      = mChunkHead;

        mChunkHead = newChunk;
        return cursor;
    }

    void * addNewChunk(size_t nChunkCapacity, size_t size) {
        ChunkHead * newChunk = reinterpret_cast<ChunkHead *>(AllocatorType::malloc(nChunkCapacity));
        jimi_assert(newChunk != NULL);

        void * cursor = reinterpret_cast<void *>(newChunk + 1);

        // Do not handle out-of-memory explicitly.
        newChunk->cursor    = reinterpret_cast<void *>(reinterpret_cast<char *>(cursor) + size);
        newChunk->remain    = nChunkCapacity - (sizeof(ChunkHead) + size);
        newChunk->capacity  = nChunkCapacity;
        newChunk->next      = mChunkHead;

        mChunkHead = newChunk;
        return cursor;
    }

public:
    void * allocate(size_t size) {
        void * buffer;
        // Default alignment size is 8
        size = JIMI_ALIGNED_TO(size, JSONFX_MALLOC_ALIGNMENT_SIZE);
        // If the chunk has enough space to allocate size bytes
        if (size <= mChunkHead->remain) {
            buffer = reinterpret_cast<void *>(mChunkHead->cursor);
            jimi_assert(buffer != NULL);

            //JIMI_PVOID_ADD(mChunkHeader->cursor, size);
            mChunkHead->cursor = reinterpret_cast<void *>(reinterpret_cast<char *>(mChunkHead->cursor) + size);

            mChunkHead->remain -= size;
            jimi_assert((ssize_t)mChunkHead->remain >= 0);
            return buffer;
        }
        else {
#if !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) || (JSONFX_ALLOW_ALLOC_BIGSIZE == 0)
            // Add a default capacity size chunk
            buffer = addNewChunk();
#else  /* !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */
            // Add a default capacity size chunk
            if (size <= kDefaultChunkCapacity) {
                buffer = addNewChunk();
            }
            else {
                size_t newAllocSize = size + sizeof(ChunkInfo);
                size_t newChunkCapacity = internal::RoundToPowerOf2(newAllocSize);
                buffer = addNewChunk(newChunkCapacity);
            }
#endif  /* defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */

            jimi_assert(buffer != NULL);
            return buffer;
        }
    }

    void * reallocate(const void * ptr, size_t size, size_t new_size) {
        // Do not shrink if new size is smaller than original
        if (size >= new_size)
            return ptr;

        // Simply expand it if it is the last allocation and there is sufficient space
        if (ptr == reinterpret_cast<void *>(reinterpret_cast<char *>(mChunkHead + 1)
                + (mChunkHead->capacity - mChunkHead->remain - sizeof(ChunkInfo)) - size)) {
            size_t increment = static_cast<size_t>(new_size - size);
            increment = JIMI_ALIGNED_TO(increment, JSONFX_MALLOC_ALIGNMENT_SIZE);
            if (increment <= mChunkHead->remain) {
                mChunkHead->remain -= increment;
                return ptr;
            }
        }

        // Realloc process: allocate and copy memory, do not free original buffer.
        void * newBuffer = this->allocate(new_size);
        jimi_assert(newBuffer != NULL);     // Do not handle out-of-memory explicitly.
        return std::memcpy(newBuffer, ptr, size);
    }

    static void deallocate(void * ptr) { (void)ptr; }                           /* Do nothing! */
    static void deallocate(void * ptr, size_t size) { (void)ptr; (void)size; }  /* Do nothing! */

private:
    ChunkHead * mChunkHead;
};

template <size_t ChunkCapacity = kDefaultChunkCapacity,
          typename Allocator = DefaultAllocator>
class SimpleMemoryPoolAllocator : public PoolAllocator
{
public:
    typedef Allocator   AllocatorType;

    static const bool   kNeedFree       = false;
    static const bool   kAutoRelease    = true;
    static const size_t kChunkCapacity  = ChunkCapacity;

    struct ChunkInfo {
        ChunkInfo * next;
        size_t      capacity;
    };

    struct ChunkHead {
        void *      cursor;
        size_t      remain;
        size_t      capacity;
        ChunkInfo * head;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        size_t      sizeTotal;
        size_t      capacityTotal;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */
    };

public:
    SimpleMemoryPoolAllocator() {
        init();
    }

    ~SimpleMemoryPoolAllocator() {
        destroy();
    }

    void destroy() {
        if (this->kAutoRelease) {
            ChunkInfo * pChunkInfo = mChunkHead.head;
            while (pChunkInfo != NULL) {
                ChunkInfo * next = pChunkInfo->next;
                AllocatorType::free(pChunkInfo);
                pChunkInfo = next;
            }
            mChunkHead.head = NULL;
        }
    }

private:
    //! Copy constructor is not permitted.
    SimpleMemoryPoolAllocator(const SimpleMemoryPoolAllocator & rhs);               /* = delete */
    //! Copy assignment operator is not permitted.
    SimpleMemoryPoolAllocator& operator =(const SimpleMemoryPoolAllocator & rhs);   /* = delete */

    void init() {
        mChunkHead.cursor           = NULL;
        mChunkHead.remain           = 0;
        mChunkHead.capacity         = 0;
        mChunkHead.head             = NULL;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mChunkHead.sizeTotal        = 0;
        mChunkHead.capacityTotal    = 0;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        addNewChunk(0);
    }

    void * addNewChunk(size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(kChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next              = mChunkHead.head;
        newChunk->capacity          = kChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mChunkHead.sizeTotal       += mChunkHead.capacity - mChunkHead.remain;
        mChunkHead.capacityTotal   += kChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        void * cursor = reinterpret_cast<void *>(newChunk + 1);

        mChunkHead.cursor           = reinterpret_cast<void *>(reinterpret_cast<char *>(cursor) + size);
        mChunkHead.remain           = kChunkCapacity - (sizeof(ChunkInfo) + size);
        mChunkHead.capacity         = kChunkCapacity;

        mChunkHead.head = newChunk;
        return cursor;
    }

    void * addNewChunk(size_t nChunkCapacity, size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(nChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next              = mChunkHead.head;
        newChunk->capacity          = nChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mChunkHead.sizeTotal       += mChunkHead.capacity - mChunkHead.remain;
        mChunkHead.capacityTotal   += nChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        void * cursor = reinterpret_cast<void *>(newChunk + 1);

        mChunkHead.cursor           = reinterpret_cast<void *>(reinterpret_cast<char *>(cursor) + size);
        mChunkHead.remain           = nChunkCapacity - (sizeof(ChunkInfo) + size);
        mChunkHead.capacity         = nChunkCapacity;

        mChunkHead.head = newChunk;
        return cursor;
    }

    void * insertNewChunkToLast(size_t nChunkCapacity, size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(nChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next              = mChunkHead.head->next;
        newChunk->capacity          = nChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mChunkHead.sizeTotal       += sizeof(ChunkInfo) + size;
        mChunkHead.capacityTotal   += nChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        mChunkHead.head->next = newChunk;

        void * cursor = reinterpret_cast<void *>(newChunk + 1);
        return cursor;
    }

public:
    void * allocate(size_t size) {
        void * buffer;
        jimi_assert(mChunkHead.head != NULL);
        jimi_assert(mChunkHead.cursor != NULL);
        // Default alignment size is 8
        size = JIMI_ALIGNED_TO(size, JSONFX_MALLOC_ALIGNMENT_SIZE);
        // If the chunk has enough space to allocate size bytes
        if (size <= mChunkHead.remain) {
            buffer = reinterpret_cast<void *>(mChunkHead.cursor);
            jimi_assert(buffer != NULL);

            //JIMI_PVOID_ADD(mChunkHead.cursor, size);
            mChunkHead.cursor = reinterpret_cast<void *>(reinterpret_cast<char *>(mChunkHead.cursor) + size);

            mChunkHead.remain -= size;
            jimi_assert((ssize_t)mChunkHead.remain >= 0);
            return buffer;
        }
        else {
            jimi_assert(mChunkHead.capacity >= mChunkHead.remain);

#if !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) || (JSONFX_ALLOW_ALLOC_BIGSIZE == 0)
            // Add a default capacity size chunk
            buffer = addNewChunk();
#else  /* !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */
            // Add a default capacity size chunk
            if (size <= kDefaultChunkCapacity) {
                buffer = addNewChunk(size);
            }
            else {
                size_t newAllocSize = size + sizeof(ChunkInfo);
                size_t newChunkCapacity = internal::RoundToPowerOf2(newAllocSize);
                if ((newChunkCapacity - newAllocSize) > mChunkHead.remain)
                    buffer = addNewChunk(newChunkCapacity, size);
                else
                    buffer = insertNewChunkToLast(newAllocSize, size);
            }
#endif  /* defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */

            jimi_assert(mChunkHead.head != NULL);
            jimi_assert(mChunkHead.cursor != NULL);

            jimi_assert(buffer != NULL);
            return buffer;
        }
    }

    void * reallocate(const void * ptr, size_t size, size_t new_size) {
        // Do not shrink if new size is smaller than original
        if (size >= new_size)
            return ptr;

        // Simply expand it if it is the last allocation and there is sufficient space
        if (ptr == reinterpret_cast<void *>(reinterpret_cast<char *>(mChunkHead.head)
                + (mChunkHead.capacity - mChunkHead.remain) - size)) {
            size_t increment = static_cast<size_t>(new_size - size);
            increment = JIMI_ALIGNED_TO(increment, JSONFX_MALLOC_ALIGNMENT_SIZE);
            if (increment <= mChunkHead.remain) {
                mChunkHead.remain -= increment;
                return ptr;
            }
        }

        // Realloc process: allocate and copy memory, do not free original buffer.
        void * newBuffer = this->allocate(new_size);
        jimi_assert(newBuffer != NULL);     // Do not handle out-of-memory explicitly.
        return std::memcpy(newBuffer, ptr, size);
    }

    static void deallocate(void * ptr) { (void)ptr; }                           /* Do nothing! */
    static void deallocate(void * ptr, size_t size) { (void)ptr; (void)size; }  /* Do nothing! */

private:
    ChunkHead mChunkHead;
};

template <size_t ChunkCapacity = kDefaultChunkCapacity,
          typename Allocator = DefaultAllocator>
class FastMemoryPoolAllocator : public PoolAllocator
{
public:
    typedef Allocator   AllocatorType;

    static const bool   kNeedFree       = false;
    static const bool   kAutoRelease    = true;
    static const size_t kChunkCapacity  = ChunkCapacity;

    struct ChunkInfo {
        ChunkInfo * next;
        size_t      used;
        size_t      capacity;
        size_t      reserve;
    };

public:
    FastMemoryPoolAllocator() : mChunkHead(NULL)
#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        , mUsedTotal(0), mCapacityTotal(0)
#endif
    {
        init();
    }

    ~FastMemoryPoolAllocator() {
        destroy();
    }

    void destroy() {
        if (this->kAutoRelease) {
            ChunkInfo * pChunkInfo = mChunkHead;
            while (pChunkInfo != NULL) {
                ChunkInfo * next = pChunkInfo->next;
                AllocatorType::free(pChunkInfo);
                pChunkInfo = next;
            }
            mChunkHead = NULL;
        }
    }

private:
    //! Copy constructor is not permitted.
    FastMemoryPoolAllocator(const FastMemoryPoolAllocator & rhs);               /* = delete */
    //! Copy assignment operator is not permitted.
    FastMemoryPoolAllocator& operator =(const FastMemoryPoolAllocator & rhs);   /* = delete */

    void init() {
        //internal_init();
        addNewChunk(0);
    }

    void internal_init() {
        mChunkHead      = NULL;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mUsedTotal      = 0;
        mCapacityTotal  = kChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */
    }

    void * addNewChunk(size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(kChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next      = mChunkHead;
        newChunk->used      = sizeof(ChunkInfo) + size;
        newChunk->capacity  = kChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        if (mChunkHead != NULL) {
            mUsedTotal  += mChunkHead->used;
        }
        mCapacityTotal  += kChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        mChunkHead = newChunk;

        void * cursor = reinterpret_cast<void *>(newChunk + 1);
        return cursor;
    }

    void * addNewChunk(size_t nChunkCapacity, size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(nChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next      = mChunkHead;
        newChunk->used      = sizeof(ChunkInfo) + size;
        newChunk->capacity  = nChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mUsedTotal      += mChunkHead->used;
        mCapacityTotal  += nChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */  

        mChunkHead = newChunk;

        void * cursor = reinterpret_cast<void *>(newChunk + 1);
        return cursor;
    }

    void * insertNewChunkToLast(size_t nChunkCapacity, size_t size) {
        ChunkInfo * newChunk = reinterpret_cast<ChunkInfo *>(AllocatorType::malloc(nChunkCapacity));
        jimi_assert(newChunk != NULL);

        // Do not handle out-of-memory explicitly.
        newChunk->next      = mChunkHead->next;
        newChunk->used      = sizeof(ChunkInfo) + size;
        newChunk->capacity  = nChunkCapacity;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
        mUsedTotal      += sizeof(ChunkInfo) + size;
        mCapacityTotal  += nChunkCapacity;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */

        mChunkHead->next = newChunk;

        void * cursor = reinterpret_cast<void *>(newChunk + 1);
        return cursor;
    }

public:
    void * allocate(size_t size) {
        void * buffer;
        jimi_assert(mChunkHead != NULL);
        // Default alignment size is 8
        size = JIMI_ALIGNED_TO(size, JSONFX_MALLOC_ALIGNMENT_SIZE);
        // If the chunk has enough space to allocate size bytes
        if (mChunkHead->used + size <= mChunkHead->capacity) {
            buffer = reinterpret_cast<void *>(mChunkHead) + mChunkHead->used;
            jimi_assert(buffer != NULL);

            mChunkHead->used += size;
            jimi_assert(mChunkHead->used <= mChunkHead->capacity);
            return buffer;
        }
        else {
#if !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) || (JSONFX_ALLOW_ALLOC_BIGSIZE == 0)
            // Add a default capacity size chunk
            buffer = addNewChunk();
#else  /* !defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */
            // Add a default capacity size chunk
            if (size <= kDefaultChunkCapacity) {
                buffer = addNewChunk(size);
            }
            else {
                size_t newAllocSize = size + sizeof(ChunkInfo);
                size_t newChunkCapacity = internal::RoundToPowerOf2(newAllocSize);
                if ((newChunkCapacity - newAllocSize) > (mChunkHead->capacity - mChunkHead->used))
                    buffer = addNewChunk(newChunkCapacity, size);
                else
                    buffer = insertNewChunkToLast(newAllocSize, size);
            }
#endif  /* defined(JSONFX_ALLOW_ALLOC_BIGSIZE) */

            jimi_assert(mChunkHead.next != NULL);
            jimi_assert(mChunkHead.cursor != NULL);

            jimi_assert(buffer != NULL);
            return buffer;
        }
    }

    void * reallocate(const void * ptr, size_t size, size_t new_size) {
        // Do not shrink if new size is smaller than original
        if (size >= new_size)
            return ptr;

        // Simply expand it if it is the last allocation and there is sufficient space
        if (ptr == reinterpret_cast<void *>(reinterpret_cast<char *>(mChunkHead)
                + mChunkHead->used - size)) {
            size_t increment = static_cast<size_t>(new_size - size);
            increment = JIMI_ALIGNED_TO(increment, JSONFX_MALLOC_ALIGNMENT_SIZE);
            if (mChunkHead->used + increment <= mChunkHead->capacity) {
                mChunkHead->used += increment;
                return ptr;
            }
        }

        // Realloc process: allocate and copy memory, do not free original buffer.
        void * newBuffer = this->allocate(new_size);
        jimi_assert(newBuffer != NULL);     // Do not handle out-of-memory explicitly.
        return std::memcpy(newBuffer, ptr, size);
    }

    static void deallocate(void * ptr) { (void)ptr; }                           /* Do nothing! */
    static void deallocate(void * ptr, size_t size) { (void)ptr; (void)size; }  /* Do nothing! */

private:
    ChunkInfo * mChunkHead;

#if defined(JSONFX_ALLOCATOR_USE_PROFILE) && (JSONFX_ALLOCATOR_USE_PROFILE != 0)
    size_t      mUsedTotal;
    size_t      mCapacityTotal;
#endif  /* JSONFX_ALLOCATOR_USE_PROFILE */
};

}  // namespace JsonFx

#endif  /* !_JSONFX_ALLOCATOR_H_ */
