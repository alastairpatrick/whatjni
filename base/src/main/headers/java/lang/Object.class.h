#ifndef java_lang_Object_SENTRY_
#define java_lang_Object_SENTRY_

namespace java {
namespace lang {

// This definition of Object is sufficient to define whatjmi::array<T>, which derives from it. It is hidden to other
// modules, which will see the proper definition, including Object's methods.
class Object {
};

}  // lang
}  // java

#endif  // java_lang_Object_SENTRY_
