#ifndef WHATJNI_ARRAY_H
#define WHATJNI_ARRAY_H

#include "whatjni/traits.h"
#include "java/lang/Object.class.h"

namespace whatjni {

template <typename T> class array;

template <typename T, typename AT, typename MAP, int MODE>
class mapped_array {
    array<T>* array_;
    T* elements_;
    jsize length_;
public:
    explicit mapped_array(array<T>* array, jsize length): array_(array), length_(length) {
        if (length_ < 0) {
            length_ = get_array_length((jarray) array_);
        }
        elements_ = MAP::get((jarray) array_, nullptr);
    }

    mapped_array(mapped_array&& rhs) {
        array_ = rhs.array_;
        elements_ = rhs.elements_;
        rhs.array_ = nullptr;
        rhs.elements_ = nullptr;
    }

    mapped_array(const mapped_array&) = delete;
    mapped_array& operator=(const mapped_array&) = delete;

    ~mapped_array() {
        if (array_) {
            MAP::release((jarray) array_, elements_, MODE);
        }
    }

    AT operator[](jsize idx) {
        check_idx(idx);
        return elements_[idx];
    }

    T operator[](jsize idx) const {
        check_idx(idx);
        return elements_[idx];
    }

private:
    void check_idx(jsize idx) const {
        if (unsigned(idx) >= unsigned(length_)) {
            abort();
        }
    }
};

template <typename T>
class array : public java::lang::Object {
    struct MapRegular {
        static T* get(jarray array, jboolean* is_copy) {
            return get_array_elements<T>(array, is_copy);
        }
        static void release(jarray array, T* elements, int mode) {
            release_array_elements(array, elements, mode);
        }
    };

    struct MapCritical {
        static T* get(jarray array, jboolean* is_copy) {
            return (T*) get_primitive_array_critical(array, is_copy);
        }
        static void release(jarray array, T* elements, int mode) {
            release_primitive_array_critical(array, elements, mode);
        }
    };

public:
    static std::string get_signature() {
        return "[" + traits<T>::get_signature();
    }

    jsize get_length() {
        return get_array_length((jarray) this);
    }
    WHATJNI_IF_PROPERTY(__declspec(property(get=get_length)) jsize length;)

    void set_data(jsize idx, T value) {
        set_array_element((jarray) this, idx, traits<T>::to_base(value));
    }
    T get_data(jsize idx) {
        return traits<T>::from_base(get_array_element<typename traits<T>::BaseType>((jarray) this, idx));
    }
    WHATJNI_IF_PROPERTY(__declspec(property(get=get_data, put=set_data)) T data[];)

    mapped_array<T, T&, MapRegular, 0> map() {
        return mapped_array<T, T&, MapRegular, 0>(this, -1);
    }
    mapped_array<T, T, MapRegular, JNI_ABORT> map_read_only() {
        return mapped_array<T, T, MapRegular, JNI_ABORT>(this, -1);
    }
    mapped_array<T, T&, MapCritical, 0> map_critical(jsize length = -1) {
        return mapped_array<T, T&, MapCritical, 0>(this, length);
    }
    mapped_array<T, T, MapCritical, JNI_ABORT> map_critical_read_only(jsize length = -1) {
        return mapped_array<T, T, MapCritical, JNI_ABORT>(this, length);
    }
};

template<typename R>
array<R>* new_array(jsize size) {
    return traits<R>::new_array(size);
}

}  // namespace whatjni

#endif  // WHATJNI_ARRAY_H

