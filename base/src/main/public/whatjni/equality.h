#ifndef WHATJNI_EQUALITY_H
#define WHATJNI_EQUALITY_H

#include "whatjni/base.h"

namespace java {
namespace lang {
class Object;
}  // namespace lang
}  // namespae java

namespace whatjni {

inline bool same(java::lang::Object* a, java::lang::Object* b) {
    return is_same_object(reinterpret_cast<jobject>(a), reinterpret_cast<jobject>(b));
}

jint same_hash(java::lang::Object* obj) {
    return get_identity_hash_code(reinterpret_cast<jobject>(obj));
}

WHATJNI_BASE bool equals(java::lang::Object* a, java::lang::Object* b) {
    return is_equal_object(reinterpret_cast<jobject>(a), reinterpret_cast<jobject>(b));
}

jint equals_hash(java::lang::Object* obj) {
    return get_hash_code(reinterpret_cast<jobject>(obj));
}

}  // namespace whatjni

#endif  // WHATJNI_EQUALITY_H
