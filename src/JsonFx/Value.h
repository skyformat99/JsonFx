
#ifndef _JSONFX_VALUE_H_
#define _JSONFX_VALUE_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>
#include <iterator>

#include "jimi/basic/stdint.h"
#include "jimi/basic/stdsize.h"

#include "JsonFx/Config.h"
#include "JsonFx/CharSet.h"
#include "JsonFx/Allocator.h"
#include "JsonFx/StringRef.h"

namespace JsonFx {

namespace internal {

// Helper to wrap/convert arbitrary types to void, useful for arbitrary type matching
template <typename T> struct Void { typedef void Type; };

///////////////////////////////////////////////////////////////////////////////
// BoolType, TrueType, FalseType
//
template <bool Cond> struct BoolType {
    static const bool Value = Cond;
    typedef BoolType Type;
};
typedef BoolType<true>  TrueType;
typedef BoolType<false> FalseType;

///////////////////////////////////////////////////////////////////////////////
// SelectIf, BoolExpr, NotExpr, AndExpr, OrExpr
//
template <bool C> struct SelectIfImpl { template <typename T1, typename T2> struct Apply { typedef T1 Type; }; };
template <> struct SelectIfImpl<false> { template <typename T1, typename T2> struct Apply { typedef T2 Type; }; };
template <bool C, typename T1, typename T2> struct SelectIfCond : SelectIfImpl<C>::template Apply<T1,T2> {};
template <typename C, typename T1, typename T2> struct SelectIf : SelectIfCond<C::Value, T1, T2> {};

template <bool Cond1, bool Cond2> struct AndExprCond : FalseType {};
template <> struct AndExprCond<true, true> : TrueType {};
template <bool Cond1, bool Cond2> struct OrExprCond : TrueType {};
template <> struct OrExprCond<false, false> : FalseType {};

template <typename C> struct BoolExpr : SelectIf<C,TrueType,FalseType>::Type {};
template <typename C> struct NotExpr  : SelectIf<C,FalseType,TrueType>::Type {};
template <typename C1, typename C2> struct AndExpr : AndExprCond<C1::Value, C2::Value>::Type {};
template <typename C1, typename C2> struct OrExpr  : OrExprCond<C1::Value, C2::Value>::Type {};

///////////////////////////////////////////////////////////////////////////////
// AddConst, MaybeAddConst, RemoveConst
//
template <typename T> struct AddConst { typedef const T Type; };
template <bool IsConstify, typename T> struct MaybeAddConst : SelectIfCond<IsConstify, const T, T> {};
template <typename T> struct RemoveConst { typedef T Type; };
template <typename T> struct RemoveConst<const T> { typedef T Type; };

}  // namespace internal

enum ValueType {
    // Base value type
    kObjectType,
    kStringType,
    kNumberType,
    kTrueType,
    kFalseType,
    kArrayType,
    kNullType,

    // Last value type
    kMaxValueType = kNullType + 1,

    // Extend value type
    kBoolType = kMaxValueType,
    kIntegerType,
    kDoubleType,
    kFloatType,

    kMaxValueTypeEx,
};

enum ValueTypeMask {
    kBoolMask       = 0x000100,
    kInt8Mask       = 0x000200,
    kUInt8Mask      = 0x000400,
    kInt16Mask      = 0x000800,
    kUInt16Mask     = 0x001000,
    kInt32Mask      = 0x002000,
    kUInt32Mask     = 0x004000,
    kInt64Mask      = 0x008000,
    kUInt64Mask     = 0x010000,

    kIntegerMask    = 0x020000,
    kFloatMask      = 0x040000,
    kDoubleMask     = 0x080000,
    kNumberMask     = 0x100000,

    kStringMask     = 0x200000,
    kCopyStrMask    = 0x400000,
    kInlineStrMask  = 0x800000,

    kNumberBoolMaskBase = kNumberMask | kIntegerMask | kBoolMask,

    kNumberBoolMask     = kNumberMask | kIntegerMask | kBoolMask | kBoolType,
    kTrueMask           = kNumberMask | kIntegerMask | kBoolMask | kTrueType,
    kFalseMask          = kNumberMask | kIntegerMask | kBoolMask | kFalseType,

    kNumberIntegerMaskBase = kNumberMask | kIntegerMask,

    kNumberIntegerMask  = kNumberMask | kIntegerMask | kIntegerType,
    kNumberIntMask      = kNumberMask | kIntegerMask | kInt32Mask  | kNumberType,
    kNumberUIntMask     = kNumberMask | kIntegerMask | kUInt32Mask | kNumberType,

    kNumberInt64Mask    = kNumberMask | kIntegerMask | kInt64Mask  | kNumberType,
    kNumberUInt64Mask   = kNumberMask | kIntegerMask | kUInt64Mask | kNumberType,

    kNumberFloatMask    = kNumberMask | kFloatMask  | kNumberType,
    kNumberDoubleMask   = kNumberMask | kDoubleMask | kNumberType,

    kNumberAnyMask      = kNumberMask | kIntegerMask
                         | kInt32Mask | kUInt32Mask | kInt64Mask | kUInt64Mask
                         | kFloatMask | kDoubleMask | kNumberType,

    kConstStringMask    = kStringMask | kStringType,
    kCopyStringMask     = kStringMask | kCopyStrMask | kStringType,
    kShortStringMask    = kStringMask | kCopyStrMask | kInlineStrMask | kStringType,

    kArrayMask      = kArrayType,
    kObjectMask     = kObjectType,
    kNullMask       = kNullType,

    // Value type mask
    kTypeMask       = 0xFF
};

// Forward declaration.
template <typename Encoding, typename PoolAllocator>
class BasicValue;

template <typename Encoding, typename PoolAllocator> 
struct BasicMember
{
    BasicValue<Encoding, PoolAllocator> name;    //!< name of member (must be a string)
    BasicValue<Encoding, PoolAllocator> value;   //!< value of member.
};

template <bool IsConst, typename Encoding, typename PoolAllocator>
class BasicMemberIterator
    : public std::iterator<std::random_access_iterator_tag,
            typename internal::MaybeAddConst<IsConst, BasicMember<Encoding, PoolAllocator> >::Type>
{
    friend class BasicValue<Encoding, PoolAllocator>;
    template <bool, typename, typename> friend class BasicMemberIterator;

    typedef BasicMember<Encoding, PoolAllocator>                        PlainType;
    typedef typename internal::MaybeAddConst<IsConst, PlainType>::Type  ValueType;
    typedef std::iterator<std::random_access_iterator_tag, ValueType>   BaseType;

public:
    //! Iterator type itself
    typedef BasicMemberIterator Iterator;

    //! Constant iterator type
    typedef BasicMemberIterator<true, Encoding, PoolAllocator>  ConstIterator;
    //! Non-constant iterator type
    typedef BasicMemberIterator<false, Encoding, PoolAllocator> NonConstIterator;

    //! Pointer to (const) GenericMember
    typedef typename BaseType::pointer         Pointer;
    //! Reference to (const) GenericMember
    typedef typename BaseType::reference       Reference;
    //! Signed integer type (e.g. \c ptrdiff_t)
    typedef typename BaseType::difference_type DifferenceType;

public:
    BasicMemberIterator() : mPtr() {}
    BasicMemberIterator(const NonConstIterator & it) : mPtr(it.mPtr) {}

    ~BasicMemberIterator() {}

    Iterator & operator ++()      { ++mPtr; return *this; }
    Iterator & operator --()      { --mPtr; return *this; }
    Iterator   operator ++(int)   { Iterator old(*this); ++mPtr; return old; }
    Iterator   operator --(int)   { Iterator old(*this); --mPtr; return old; }

    Iterator operator +(DifferenceType n) const { return Iterator(mPtr + n); }
    Iterator operator -(DifferenceType n) const { return Iterator(mPtr - n); }

    Iterator & operator +=(DifferenceType n) { mPtr += n; return *this; }
    Iterator & operator -=(DifferenceType n) { mPtr -= n; return *this; }

    bool operator ==(ConstIterator that) const { return mPtr == that.mPtr; }
    bool operator !=(ConstIterator that) const { return mPtr != that.mPtr; }
    bool operator <=(ConstIterator that) const { return mPtr <= that.mPtr; }
    bool operator >=(ConstIterator that) const { return mPtr >= that.mPtr; }
    bool operator < (ConstIterator that) const { return mPtr <  that.mPtr; }
    bool operator > (ConstIterator that) const { return mPtr >  that.mPtr; }

    Reference operator *() const  { return *mPtr; }
    Pointer   operator ->() const { return  mPtr; }
    Reference operator [](DifferenceType n) const { return mPtr[n]; }

    //! Distance
    DifferenceType operator -(ConstIterator that) const { return mPtr - that.mPtr; }

private:
    //! Internal constructor from plain pointer
    explicit BasicMemberIterator(Pointer p) : mPtr(p) {}

    Pointer mPtr;
};

// Save and setting the packing alignment
#pragma pack(push)
#pragma pack(1)

template <typename Encoding = JSONFX_DEFAULT_ENCODING, typename PoolAllocator = DefaultPoolAllocator>
class BasicValue {
public:
    typedef typename Encoding::CharType     CharType;
    typedef Encoding                        EncodingType;
    typedef PoolAllocator                   PoolAllocatorType;
    typedef BasicStringRef<CharType>        StringRefType;      //!< Reference to a constant string

    typedef BasicMember<Encoding, PoolAllocator> Member;

    typedef BasicValue *                    ValueIterator;      //!< Value iterator for iterating in array.
    typedef const BasicValue *              ConstValueIterator; //!< Constant value iterator for iterating in array.

    //!< Member iterator for iterating in object.
    typedef typename BasicMemberIterator<false, Encoding, PoolAllocator>::Iterator MemberIterator;
    //!< Constant member iterator for iterating in object.
    typedef typename BasicMemberIterator<true,  Encoding, PoolAllocator>::Iterator ConstMemberIterator;

    typedef uint32_t                        SizeType;
    typedef uint32_t                        ValueType;

public:
    BasicValue() : mValueType(kNullMask), mValueData() {}

    BasicValue(const CharType * str) {
        mValueType = kStringMask;
        mValueData.str.data = str;
        mValueData.str.size = ::strlen(str);
    }

    explicit BasicValue(StringRefType str) : mValueData(), mValueType() { setStringRaw(str); }

    ~BasicValue() { release(); }

private:
    //! Copy constructor is not permitted.
    BasicValue(const BasicValue & rhs);

public:
    void visit();
    void release();

    void setStringRaw(StringRefType str) {
        mValueType = kConstStringMask;
        mValueData.str.data = str.mData;
        mValueData.str.size = str.mSize;
    }

    void setObject() {
        mValueType = kObjectMask;
        mValueData.obj.members = NULL;
        mValueData.obj.size = 0;
        mValueData.obj.capacity = 0;
    }

    ValueType getType() const { return static_cast<ValueType>(mValueType & kTypeMask); }

    bool isNull()   const { return (mValueType == kNullMask);               }
    bool isFalse()  const { return (mValueType == kFalseMask);              }
    bool isTrue()   const { return (mValueType == kTrueMask);               }
    bool isBool()   const { return ((mValueType & kBoolMask) != 0);         }
    bool isObject() const { return (mValueType == kObjectMask);             }
    bool isArray()  const { return (mValueType == kArrayMask);              }
    bool isNumber() const { return ((mValueType & kNumberMask) != 0);       }
    bool isInt()    const { return ((mValueType & kNumberIntMask) != 0);    }
    bool isUint()   const { return ((mValueType & kNumberUIntMask) != 0);   }
    bool isInt64()  const { return ((mValueType & kInt64Mask) != 0);        }
    bool isUint64() const { return ((mValueType & kUInt64Mask) != 0);       }
    bool isFloat()  const { return ((mValueType & kDoubleMask) != 0);       }
    bool isDouble() const { return ((mValueType & kDoubleMask) != 0);       }
    bool isString() const { return ((mValueType & kStringMask) != 0);       }

    //MemberIterator findMember(const CharType * name) { return MemberIterator(NULL); }
    MemberIterator findMember(const CharType * name) {
        BasicValue n(StringRefType(name).mData);
        return findMember(n);
    }

    ConstMemberIterator findMember(const CharType * name) const {
        return const_cast<BasicValue &>(*this).findMember(name);
    }

    template <typename SourceAllocator>
    MemberIterator findMember(const BasicValue<Encoding, SourceAllocator> & name) {
        jimi_assert(isObject());
        jimi_assert(name.isString());
        MemberIterator member = getMemberBegin();
        for ( ; member != getMemberEnd(); ++member)
            if (name.stringEqual(member->name)) {
                break;
            }
        return member;
    }

    template <typename SourceAllocator>
    ConstMemberIterator findMember(const BasicValue<Encoding, SourceAllocator> & name) const {
        return const_cast<BasicValue &>(*this).findMember(name);
    }

    MemberIterator getMemberBegin() { jimi_assert(isObject()); return MemberIterator(mValueData.obj.members); }
    MemberIterator getMemberEnd()   { jimi_assert(isObject()); return MemberIterator(mValueData.obj.members + mValueData.obj.size); }

    ConstMemberIterator getMemberBegin() const { jimi_assert(isObject()); return ConstMemberIterator(mValueData.obj.members); }
    ConstMemberIterator getMemberEnd() const   { jimi_assert(isObject()); return ConstMemberIterator(mValueData.obj.members + mValueData.obj.size); }

    bool hasMember(const CharType * name) const { return (findMember(name) != getMemberEnd()); }

    template <typename SourceAllocator>
    bool hasMember(const BasicValue<Encoding, SourceAllocator> & name) const { return findMember(name) != getMemberEnd(); }

    template <typename SourceAllocator>
    BasicValue & operator[] (const BasicValue<Encoding, SourceAllocator> & name) {
        MemberIterator member = findMember(name);
        if (member != getMemberEnd()) {
            return member->value;
        }
        else {
            // See above note
            jimi_assert(false);
            static BasicValue nullValue;
            return NullValue;
        }
    }


    template <typename SourceAllocator>
    bool stringEqual(const BasicValue<Encoding, SourceAllocator> & rhs) const {
        jimi_assert(isString());
        jimi_assert(rhs.isString());

        const SizeType len1 = getStringLength();
        const SizeType len2 = rhs.getStringLength();
        if (len1 != len2) { return false; }

        const CharType * const str1 = getString();
        const CharType * const str2 = rhs.getString();
        // fast path for constant string
        if (str1 == str2) { return true; }

        return (std::memcmp(str1, str2, sizeof(CharType) * len1) == 0);
    }

    const CharType * getString() const { jimi_assert(isString()); return ((mValueType & kInlineStrMask) ? mValueData.sso.data : mValueData.str.data); }

    SizeType getStringLength() const { jimi_assert(isString()); return ((mValueType & kInlineStrMask) ? (mValueData.sso.GetLength()) : mValueData.str.size); }

public:
    union Number {
        int64_t  i64;
        uint64_t u64;
        float    f;
        double   d;
    };

    struct String {
        const CharType *data;
        SizeType        size;
        SizeType        capacity;
        unsigned int    hashCode;
    };

    struct ShortString {
        const CharType *data;
        SizeType        size;
        SizeType        capacity;
        unsigned int    hashCode;

        SizeType GetLength() const { return size; }
    };

    struct Element {
        void * data;
    };

    struct Array {
        BasicValue *    items;
        SizeType        size;
        SizeType        capacity;
        unsigned int    hashCode;
    };

    struct Object {
        Member *        members;
        SizeType        size;
        SizeType        capacity;
        unsigned int    hashCode;
    };

    union ValueData
    {
        String      str;
        ShortString sso;
        Number      num;
        Array       array;
        Object      obj;
    };

private:
    ValueType   mValueType;
    ValueData   mValueData;
};

template <typename Encoding, typename PoolAllocator>
void BasicValue<Encoding, PoolAllocator>::release()
{
    //printf("JsonFx::BasicValue::release() enter.\n");
    // Shortcut by Allocator's trait
    if (PoolAllocatorType::kNeedFree) {
        switch (mValueType) {
        case kArrayMask:
            for (BasicValue * v = mValueData.array.items; v != mValueData.array.items + mValueData.array.size; ++v) {
                v->~BasicValue();
            }
            PoolAllocatorType::free(mValueData.array.items);
            break;

        case kObjectMask:
            for (MemberIterator m = getMemberBegin(); m != getMemberEnd(); ++m) {
                m->~Member();
            }
            PoolAllocatorType::free(mValueData.obj.members);
            break;

        case kCopyStrMask:
            PoolAllocatorType::free(const_cast<CharType *>(mValueData.str.data));
            break;

        default:
            printf("JsonFx::BasicValue::release() -- switch(mValueType) branch = default\n");
            break;  // Do nothing for other types.
        }
    }
    //printf("JsonFx::BasicValue::release() over.\n");
}

template <typename Encoding, typename PoolAllocator>
void BasicValue<Encoding, PoolAllocator>::visit()
{
    printf("JsonFx::Value::visit() visited.\n\n");
}

// Recover the packing alignment
#pragma pack(pop)

// Define default Value class type
typedef BasicValue<>   Value;

}  // namespace JsonFx

#endif  /* !_JSONFX_VALUE_H_ */