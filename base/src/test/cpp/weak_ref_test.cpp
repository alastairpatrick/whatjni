#include "whatjni/weak_ref.h"
#include "whatjni/local_frame.h"

#include "gtest/gtest.h"

#include <algorithm>

namespace whatjni {

namespace {

// For weak_ref<T>, there are some specific operations that require T be a complete type but, generally speaking, T may
// be an incomplete type so we test with an incomplete type.
struct Point;

struct Base {
};
struct Derived: Base {
};

}  // namespace anonymous

struct WeakRefTest: testing::Test {
    WeakRefTest() {
        clazz = find_class("java/awt/Point");
        obj1 = (Point*) alloc_object(clazz);
        obj2 = (Point*) alloc_object(clazz);
        derivedObj = (Derived*) alloc_object(clazz);
    }

    local_frame frame;
    jclass clazz;
    Point* obj1;
    Point* obj2;
    Derived* derivedObj;
};

TEST_F(WeakRefTest, defaults_to_null) {
    weak_ref<Point> ref;
    EXPECT_FALSE(ref.lock());
}

TEST_F(WeakRefTest, can_copy_construct_refs) {
    weak_ref<Point> ref1(obj1);
    weak_ref<Point> ref2(ref1);
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref1.lock()));
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref2.lock()));
}

TEST_F(WeakRefTest, can_assign_refs) {
    weak_ref<Point> ref1(obj1);
    weak_ref<Point> ref2;
    ref2 = ref1;
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref1.lock()));
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref2.lock()));
}

TEST_F(WeakRefTest, can_assign_self) {
    weak_ref<Point> ref1(obj1);
    ref1 = ref1;
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref1.lock()));
}

TEST_F(WeakRefTest, can_move_construct_refs) {
    weak_ref<Point> ref1(obj1);
    weak_ref<Point> ref2(std::move(ref1));
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref2.lock()));
    EXPECT_FALSE(ref1.lock());
}

TEST_F(WeakRefTest, can_move_assign_refs) {
    weak_ref<Point> ref1(obj1);
    weak_ref<Point> ref2;
    ref2 = std::move(ref1);
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref2.lock()));
    EXPECT_FALSE(ref1.lock());
}

TEST_F(WeakRefTest, can_move_assign_self) {
    weak_ref<Point> ref1(obj1);
    ref1 = std::move(ref1);
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref1.lock()));
}

TEST_F(WeakRefTest, can_swap_refs) {
    weak_ref<Point> ref1(obj1);
    weak_ref<Point> ref2(obj2);
    weak_ref<Point> ref3(ref1);
    weak_ref<Point> ref4(ref2);
    std::swap(ref3, ref4);
    EXPECT_TRUE(is_same_object((jobject) obj1, (jobject) ref4.lock()));
    EXPECT_TRUE(is_same_object((jobject) obj2, (jobject) ref3.lock()));
}

TEST_F(WeakRefTest, can_construct_refs_with_derived_type) {
    weak_ref<Base> ref1(derivedObj);
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref1.lock()));
}

TEST_F(WeakRefTest, can_copy_construct_refs_with_derived_type) {
    weak_ref<Derived> ref1(derivedObj);
    weak_ref<Base> ref2(ref1);
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref1.lock()));
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref2.lock()));
}

TEST_F(WeakRefTest, can_move_construct_refs_with_derived_type) {
    weak_ref<Derived> ref1(derivedObj);
    weak_ref<Base> ref2(std::move(ref1));
    EXPECT_FALSE(ref1.lock());
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref2.lock()));
}

TEST_F(WeakRefTest, can_assign_refs_with_derived_type) {
    weak_ref<Derived> ref1(derivedObj);
    weak_ref<Base> ref2;
    ref2 = ref1;
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref1.lock()));
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref2.lock()));
}

TEST_F(WeakRefTest, can_move_assign_refs_with_derived_type) {
    weak_ref<Derived> ref1(derivedObj);
    weak_ref<Base> ref2;
    ref2 = std::move(ref1);
    EXPECT_FALSE(ref1.lock());
    EXPECT_TRUE(is_same_object((jobject) derivedObj, (jobject) ref2.lock()));
}

}  // namespace whatjni
