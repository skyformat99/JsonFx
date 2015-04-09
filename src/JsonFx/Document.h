
#ifndef _JSONFX_DOCUMENT_H_
#define _JSONFX_DOCUMENT_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>

#include "JsonFx/Config.h"
#include "JsonFx/CharSet.h"
#include "JsonFx/Allocator.h"
#include "JsonFx/Value.h"

#include "jimi/basic/assert.h"

namespace JsonFx {

template <typename Encoding = JSONFX_DEFAULT_ENCODING,
          typename PoolAllocator = DefaultPoolAllocator,
          typename StackAllocator = DefaultStackAllocator>
class BasicDocument : public BasicValue<Encoding, PoolAllocator>
{
public:
    typedef typename Encoding::CharType         CharType;           //!< Character type derived from Encoding.
    typedef BasicValue<Encoding, PoolAllocator> ValueType;          //!< Value type of the document.
    typedef Encoding                            EncodingType;       //!< Character encoding type.
    typedef PoolAllocator                       PoolAllocatorType;  //!< Pool allocator type from template parameter.
    typedef StackAllocator                      StackAllocatorType; //!< Stack allocator type from template parameter.

public:
    BasicDocument(const PoolAllocatorType *poolAllocator = NULL)
        : mPoolAllocator(poolAllocator), mPoolAllocatorNeedFree(false) {
        initPoolAllocator(poolAllocator);
    }

    ~BasicDocument() {
        destroy();
    }

private:
    //! Prohibit copying
    BasicDocument(const BasicDocument &);
    //! Prohibit assignment
    BasicDocument & operator =(const BasicDocument &);

public:
    const PoolAllocatorType * getAllocator() const { return mPoolAllocator; }

    BasicDocument & parse(const CharType * text);

    void visit();

    void test() {
        printf("JsonFx::BasicDocument::test()\n\n");
    }

private:
    void destroy() {
        if (this->mPoolAllocatorNeedFree) {
            if (this->mPoolAllocator) {
                delete this->mPoolAllocator;
                this->mPoolAllocator = NULL;
            }
        }
    }

    void initPoolAllocator(const PoolAllocatorType *poolAllocator) {
        if (poolAllocator == NULL) {
            const PoolAllocatorType * newPoolAllocator = new PoolAllocatorType();
            jimi_assert(newPoolAllocator != NULL);
            if (newPoolAllocator != NULL) {
                mPoolAllocator = newPoolAllocator;
                mPoolAllocatorNeedFree = true;
            }
        }
    }

private:    
    const PoolAllocatorType *   mPoolAllocator;
    bool                        mPoolAllocatorNeedFree;
};

// Define default Document class type
typedef BasicDocument<>     Document;

template <typename Encoding, typename PoolAllocator, typename StackAllocator>
void BasicDocument<Encoding, PoolAllocator, StackAllocator>::visit()
{
    printf("JsonFx::BasicDocument::visit() visited.\n\n");
}

template <typename Encoding, typename PoolAllocator, typename StackAllocator>
BasicDocument<Encoding, PoolAllocator, StackAllocator> &
BasicDocument<Encoding, PoolAllocator, StackAllocator>::parse(const CharType * text)
{
    jimi_assert(text != NULL);
    printf("JsonFx::BasicDocument::parse() visited.\n\n");

    setObject();
    return *this;
}

}  // namespace JsonFx

#endif  /* !_JSONFX_DOCUMENT_H_ */