#include "whatjni/ref.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <unordered_map>

// TODO: remove once automatically generated
namespace java {
namespace lang {

class String {
public:
    std::u16string to_std_string() {
        // TODO: with C++17, getStringUTFRegion could copy directly into std::string buffer.
        struct Mapper {
            jstring s;
            jsize length;
            const jchar* chars;

            Mapper(jstring s): s(s) {
                length = whatjni::get_string_length(s);
                jboolean is_copy;
                chars = whatjni::get_string_chars(s, &is_copy);
            }
            ~Mapper() {
                whatjni::release_string_chars(s, chars);
            }
        };

        Mapper mapper((jstring) this);
        return std::u16string((const char16_t*) mapper.chars, mapper.length);
    }
};

}  // lang
}  // java

namespace whatjni {

namespace {

// For ref<T>, there are some specific operations that require T be a complete type but, generally speaking, T may be an
// incomplete type so we test with an incomplete type.
struct Vector2;

struct Base {
};
struct Derived: Base {
};

}  // namespace anonymous


struct RefTest: testing::Test {
    RefTest() {
        push_local_frame(16);
        clazz = find_class("mymodule/Vector2");
        obj1 = (Vector2*) alloc_object(clazz);
        obj2 = (Vector2*) alloc_object(clazz);
        derivedObj = (Derived*) alloc_object(clazz);
    }

    ~RefTest() {
        pop_local_frame();
    }

    jclass clazz;
    Vector2* obj1;
    Vector2* obj2;
    Derived* derivedObj;
};

TEST_F(RefTest, defaults_to_null) {
    ref<Vector2> ref1, ref2;
    EXPECT_FALSE(ref1);
    EXPECT_TRUE(ref1 == nullptr);
    EXPECT_TRUE(nullptr == ref1);
    EXPECT_TRUE(ref1 == ref2);
    EXPECT_FALSE(ref1 != nullptr);
    EXPECT_FALSE(nullptr != ref1);
    EXPECT_FALSE(ref1 != ref2);
    EXPECT_EQ(ref1.operator->(), nullptr);
}

TEST_F(RefTest, takes_ownership_of_local_ref) {
    ref<Vector2> ref1(obj1, own_ref);
    EXPECT_TRUE(ref1);
    EXPECT_FALSE(ref1 == nullptr);
    EXPECT_FALSE(nullptr == ref1);
    EXPECT_TRUE(ref1 == ref1);
    EXPECT_NE(ref1.operator->(), nullptr);
}

TEST_F(RefTest, distinct_refs_to_the_same_object_are_equal) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj1);
    EXPECT_TRUE(ref1 == ref2);
    EXPECT_FALSE(ref1 != ref2);
}

TEST_F(RefTest, refs_to_different_objects_are_not_equal) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj2);
    EXPECT_FALSE(ref1 == ref2);
    EXPECT_TRUE(ref1 != ref2);
}

TEST_F(RefTest, can_copy_construct_refs) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(ref1);
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref1 == ref2);
}

TEST_F(RefTest, can_assign_refs) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2;
    ref2 = ref1;
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref1 == ref2);
}

TEST_F(RefTest, can_assign_self) {
    ref<Vector2> ref1(obj1);
    ref1 = ref1;
    EXPECT_TRUE(ref1);
}

TEST_F(RefTest, can_move_construct_refs) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(std::move(ref1));
    EXPECT_TRUE(ref2);
    EXPECT_FALSE(ref1);
}

TEST_F(RefTest, can_move_assign_refs) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2;
    ref2 = std::move(ref1);
    EXPECT_TRUE(ref2);
    EXPECT_FALSE(ref1);
}

TEST_F(RefTest, can_move_assign_self) {
    ref<Vector2> ref1(obj1);
    ref1 = std::move(ref1);
    EXPECT_TRUE(ref1);
}

TEST_F(RefTest, can_swap_refs) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj2);
    ref<Vector2> ref3(ref1);
    ref<Vector2> ref4(ref2);
    std::swap(ref3, ref4);
    EXPECT_EQ(ref1, ref4);
    EXPECT_EQ(ref2, ref3);
}

TEST_F(RefTest, can_access_members) {
    ref<Vector2> ref1(obj1, own_ref);
    EXPECT_EQ(obj1, ref1.operator->());
}

TEST_F(RefTest, can_construct_refs_with_derived_type) {
    ref<Base> ref1(derivedObj);
    EXPECT_TRUE(ref1);
}

TEST_F(RefTest, can_copy_construct_refs_with_derived_type) {
    ref<Derived> ref1(derivedObj);
    ref<Base> ref2(ref1);
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref2);
}

TEST_F(RefTest, can_move_construct_refs_with_derived_type) {
    ref<Derived> ref1(derivedObj);
    ref<Base> ref2(std::move(ref1));
    EXPECT_FALSE(ref1);
    EXPECT_TRUE(ref2);
}

TEST_F(RefTest, can_assign_refs_with_derived_type) {
    ref<Derived> ref1(derivedObj);
    ref<Base> ref2;
    ref2 = ref1;
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref2);
}

TEST_F(RefTest, can_move_assign_refs_with_derived_type) {
    ref<Derived> ref1(derivedObj);
    ref<Base> ref2;
    ref2 = std::move(ref1);
    EXPECT_FALSE(ref1);
    EXPECT_TRUE(ref2);
}

TEST_F(RefTest, can_use_ref_as_key_in_unordered_collection_with_identity_equality) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj2);
    ref<Vector2> ref3;

    std::unordered_map<ref<Vector2>, int, by_identity<Vector2>, by_identity<Vector2>> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}

TEST_F(RefTest, can_use_ref_as_key_in_unordered_collection_with_value_equality) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj2);
    ref<Vector2> ref3;

    std::unordered_map<ref<Vector2>, int, by_value<Vector2>, by_value<Vector2>> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}

TEST_F(RefTest, can_use_ref_as_key_in_unordered_collection_with_default_identity_equality) {
    ref<Vector2> ref1(obj1);
    ref<Vector2> ref2(obj2);
    ref<Vector2> ref3;

    std::unordered_map<ref<Vector2>, int> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}

TEST_F(RefTest, c_string_to_object_ref) {
    ref<java::lang::String> str(u"hello");
    EXPECT_EQ(str->to_std_string(), u"hello");

    str = u"foo";
    EXPECT_EQ(str->to_std_string(), u"foo");
}

TEST_F(RefTest, std_string_to_object_ref) {
    ref<java::lang::String> str(std::u16string(u"hello"));
    EXPECT_EQ(str->to_std_string(), u"hello");

    str = std::u16string(u"foo");
    EXPECT_EQ(str->to_std_string(), u"foo");
}

}  // namespace whatjni
