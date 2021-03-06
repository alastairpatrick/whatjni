#include "whatjni/ref.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <unordered_map>

/*
std::string to_std_string() {
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
    std::string result;
    utf8::utf16to8(mapper.chars, mapper.chars + mapper.length, std::back_inserter(result));
    return result;
}
*/

namespace whatjni {

namespace {

// For ref<T>, there are some specific operations that require T be a complete type but, generally speaking, T may be an
// incomplete type so we test with an incomplete type.
struct Point;

struct Base {
};

struct Derived {
};

inline void static_assert_instanceof(Derived*, Base*) {}

}  // namespace anonymous


struct RefTest: testing::Test {
    RefTest() {
        push_local_frame(16);
        clazz = find_class("java/awt/Point");
        obj1 = (Point*) alloc_object(clazz);
        obj2 = (Point*) alloc_object(clazz);
        derivedObj = (Derived*) alloc_object(clazz);
    }

    ~RefTest() {
        pop_local_frame();
    }

    jclass clazz;
    Point* obj1;
    Point* obj2;
    Derived* derivedObj;
};

TEST_F(RefTest, defaults_to_null) {
    ref<Point> ref1, ref2;
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
    ref<Point> ref1(obj1, own_ref);
    EXPECT_TRUE(ref1);
    EXPECT_FALSE(ref1 == nullptr);
    EXPECT_FALSE(nullptr == ref1);
    EXPECT_TRUE(ref1 == ref1);
    EXPECT_NE(ref1.operator->(), nullptr);
}

TEST_F(RefTest, distinct_refs_to_the_same_object_are_equal) {
    ref<Point> ref1(obj1);
    ref<Point> ref2(obj1);
    EXPECT_TRUE(ref1 == ref2);
    EXPECT_FALSE(ref1 != ref2);
}

TEST_F(RefTest, refs_to_different_objects_are_not_equal) {
    ref<Point> ref1(obj1);
    ref<Point> ref2(obj2);
    EXPECT_FALSE(ref1 == ref2);
    EXPECT_TRUE(ref1 != ref2);
}

TEST_F(RefTest, can_copy_construct_refs) {
    ref<Point> ref1(obj1);
    ref<Point> ref2(ref1);
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref1 == ref2);
}

TEST_F(RefTest, can_assign_refs) {
    ref<Point> ref1(obj1);
    ref<Point> ref2;
    ref2 = ref1;
    EXPECT_TRUE(ref1);
    EXPECT_TRUE(ref1 == ref2);
}

TEST_F(RefTest, can_assign_self) {
    ref<Point> ref1(obj1);
    ref1 = ref1;
    EXPECT_TRUE(ref1);
}

TEST_F(RefTest, can_move_construct_refs) {
    ref<Point> ref1(obj1);
    ref<Point> ref2(std::move(ref1));
    EXPECT_TRUE(ref2);
    EXPECT_FALSE(ref1);
}

TEST_F(RefTest, can_move_assign_refs) {
    ref<Point> ref1(obj1);
    ref<Point> ref2;
    ref2 = std::move(ref1);
    EXPECT_TRUE(ref2);
    EXPECT_FALSE(ref1);
}

TEST_F(RefTest, can_move_assign_self) {
    ref<Point> ref1(obj1);
    ref1 = std::move(ref1);
    EXPECT_TRUE(ref1);
}

TEST_F(RefTest, can_swap_refs) {
    ref<Point> ref1(obj1);
    ref<Point> ref2(obj2);
    ref<Point> ref3(ref1);
    ref<Point> ref4(ref2);
    std::swap(ref3, ref4);
    EXPECT_EQ(ref1, ref4);
    EXPECT_EQ(ref2, ref3);
}

TEST_F(RefTest, can_access_members) {
    ref<Point> ref1(obj1, own_ref);
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
    ref<Point> ref1(obj1);
    ref<Point> ref2(obj2);
    ref<Point> ref3;

    std::unordered_map<ref<Point>, int, by_identity<Point>, by_identity<Point>> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}

TEST_F(RefTest, can_use_ref_as_key_in_unordered_collection_with_value_equality) {
    jfieldID field = get_field_id(clazz, "x", "I");

    ref<Point> ref1(obj1);
    set_field((jobject) ref1.operator->(), field, jint(1));

    ref<Point> ref2(obj2);
    set_field((jobject) ref2.operator->(), field, jint(2));

    ref<Point> ref3;

    std::unordered_map<ref<Point>, int, by_value<Point>, by_value<Point>> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}

TEST_F(RefTest, can_use_ref_as_key_in_unordered_collection_with_default_value_equality) {
    jfieldID field = get_field_id(clazz, "x", "I");

    ref<Point> ref1(obj1);
    set_field((jobject) ref1.operator->(), field, jint(1));

    ref<Point> ref2(obj2);
    set_field((jobject) ref2.operator->(), field, jint(2));

    ref<Point> ref3;

    std::unordered_map<ref<Point>, int> map;
    map[ref1] = 1;
    map[ref2] = 2;
    map[ref3] = 3;

    ASSERT_EQ(map[ref1], 1);
    ASSERT_EQ(map[ref2], 2);
    ASSERT_EQ(map[ref3], 3);
}
/*
TEST_F(RefTest, c_string_to_object_ref) {
    ref<java::lang::String> str("hello");
    EXPECT_EQ(str->to_std_string(), "hello");

    str = "foo";
    EXPECT_EQ(str->to_std_string(), "foo");
}

TEST_F(RefTest, std_string_to_object_ref) {
    ref<java::lang::String> str(std::string("hello"));
    EXPECT_EQ(str->to_std_string(), "hello");

    str = std::string("foo");
    EXPECT_EQ(str->to_std_string(), "foo");
}
*/
}  // namespace whatjni
