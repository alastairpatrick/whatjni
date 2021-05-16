#ifndef WHATJNI_LOCAL_FRAME_H
#define WHATJNI_LOCAL_FRAME_H

#include "whatjni/base.h"

#include <cassert>

namespace whatjni {

class local_frame {
    bool popped = false;
public:
    explicit local_frame(jint size = 16) {
        push_local_frame(size);
    }

    local_frame& operator=(const local_frame&) = delete;

    template <typename T>
    T* pop(T* result) {
        assert(!popped);
        popped = true;
        return (T*) pop_local_frame((jobject) result);
    }

    ~local_frame() {
        if (!popped) {
            pop_local_frame(nullptr);
        }
    }
};

}  // namespace whatjni

#endif  // WHATJNI_LOCAL_FRAME_H
