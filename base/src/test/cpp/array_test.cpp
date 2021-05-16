#include "whatjni/array.h"
#include "whatjni/local_frame.h"
#include "whatjni/equality.h"
#include "java/lang/Object.class.h"

#include "gtest/gtest.h"

namespace whatjni {

namespace {

struct Point : java::lang::Object {
    static std::string get_signature() {
        return "Ljava/awt/Point;";
    }
};

}  // namespace anonymous

struct ArrayTest: testing::Test {
    ArrayTest() {
        pointClass = find_class("java/awt/Point");
        int_array = new_array<jint>(3);
        obj_array = new_array<Point*>(3);
        nested_int_array = new_array<array<jint>*>(1);
        nested_obj_array = new_array<array<Point*>*>(1);
    }

    local_frame frame;
    jclass pointClass;
    array<jint>* int_array;
    array<Point*>* obj_array;
    array<array<jint>*>* nested_int_array;
    array<array<Point*>*>* nested_obj_array;
};

TEST_F(ArrayTest, new_primitive_array) {
    EXPECT_TRUE(int_array);
    EXPECT_EQ(int_array->get_length(), 3);
    WHATJNI_IF_PROPERTY(EXPECT_EQ(int_array->length, 3));
}

TEST_F(ArrayTest, set_then_get_int_element) {
    for (int i = 0; i < int_array->get_length(); ++i) {
        int_array->set_data(i, i);
    }
    for (int i = 0; i < int_array->get_length(); ++i) {
        EXPECT_EQ(int_array->get_data(i), i);
    }

    WHATJNI_IF_PROPERTY({
        for (int i = 0; i < int_array->length; ++i) {
            int_array->data[i] = i;
        }
        for (int i = 0; i < int_array->length; ++i) {
            EXPECT_EQ(int_array->data[i], i);
        }
    })
}

TEST_F(ArrayTest, access_mapped_elements) {
    jsize len = int_array->get_length();
    {
        auto mapped = int_array->map();
        for (int i = 0; i < len; ++i) {
            mapped[i] = i;
        }
    }
    for (int i = 0; i < len; ++i) {
        EXPECT_EQ(int_array->get_data(i), i);
    }
}

TEST_F(ArrayTest, access_read_only_mapped_elements) {
    jsize len = int_array->get_length();
    for (int i = 0; i < len; ++i) {
        int_array->set_data(i, i);
    }
    auto mapped = int_array->map_read_only();
    for (int i = 0; i < len; ++i) {
        EXPECT_EQ(mapped[i], i);
    }
}

TEST_F(ArrayTest, access_critically_mapped_elements) {
    jsize len = int_array->get_length();
    {
        auto mapped = int_array->map_critical();
        for (int i = 0; i < len; ++i) {
            mapped[i] = i;
        }
    }
    for (int i = 0; i < len; ++i) {
        EXPECT_EQ(int_array->get_data(i), i);
    }
}

TEST_F(ArrayTest, access_read_only_critically_mapped_elements) {
    jsize len = int_array->get_length();
    for (int i = 0; i < len; ++i) {
        int_array->set_data(i, i);
    }
    auto mapped = int_array->map_critical_read_only();
    for (int i = 0; i < len; ++i) {
        EXPECT_EQ(mapped[i], i);
    }
}

TEST_F(ArrayTest, object_array) {
    EXPECT_TRUE(obj_array);
    EXPECT_EQ(obj_array->get_length(), 3);
}

TEST_F(ArrayTest, set_then_get_object_element) {
    Point* obj = reinterpret_cast<Point*>(alloc_object(pointClass));

    for (int i = 0; i < obj_array->get_length(); ++i) {
        obj_array->set_data(i, obj);
    }
    for (int i = 0; i < obj_array->get_length(); ++i) {
        EXPECT_TRUE(same(obj_array->get_data(i), obj));
    }
}

TEST_F(ArrayTest, nested_primitive_array) {
    EXPECT_TRUE(nested_int_array);
    EXPECT_EQ(nested_int_array->get_length(), 1);
}

TEST_F(ArrayTest, set_then_get_nested_int_element) {
    nested_int_array->set_data(0, int_array);
    EXPECT_TRUE(same(nested_int_array->get_data(0), int_array));
}

TEST_F(ArrayTest, nested_obj_array) {
    EXPECT_TRUE(nested_obj_array);
    EXPECT_EQ(nested_obj_array->get_length(), 1);
}

TEST_F(ArrayTest, set_then_get_nested_object_element) {
    nested_obj_array->set_data(0, obj_array);
    EXPECT_TRUE(same(nested_obj_array->get_data(0), obj_array));
}

}  // namespace whatjni
