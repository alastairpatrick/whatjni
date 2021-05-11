#ifndef WHATJNI_NO_DESTROY_H
#define WHATJNI_NO_DESTROY_H

namespace whatjni {

template <class T>
class no_destroy
{
    alignas(T) unsigned char data_[sizeof(T)];
public:
    template <class... Ts> no_destroy(Ts&&... ts) {
        new (data_) T(std::forward<Ts>(ts)...);
    }

    T &get() {
        return *reinterpret_cast<T *>(data_);
    }
};

}  // namespace whatjni

#endif  // WHATJNI_NO_DESTROY_H
