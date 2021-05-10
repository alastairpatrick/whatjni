#include "whatjni/base.h"

#include "gtest/gtest.h"

namespace whatjni {

struct BaseTest: testing::Test {
    BaseTest() {
        push_local_frame(16);
    }

    ~BaseTest() {
        pop_local_frame();
    }
};

TEST_F(BaseTest, get_super_class) {
    auto clazz1 = find_class("java/lang/String");
    auto clazz2 = find_class("java/lang/Object");
    EXPECT_TRUE(is_same_object(get_super_class(clazz1), clazz2));
    EXPECT_TRUE(is_assignable_from(clazz1, clazz2));
}

TEST_F(BaseTest, alloc_object) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    EXPECT_NE(obj, nullptr);
}

TEST_F(BaseTest, is_instance_of) {
    auto clazz1 = find_class("java/awt/Point");
    auto clazz2 = find_class("java/lang/Object");
    jobject obj = alloc_object(clazz1);
    EXPECT_TRUE(is_instance_of(obj, clazz1));
    EXPECT_TRUE(is_instance_of(obj, clazz2));
}

TEST_F(BaseTest, new_object) {
    auto clazz = find_class("java/awt/Point");
    auto constructor_id = get_method_id(clazz, "<init>", "()V");
    jobject obj = new_object(clazz, constructor_id);
    EXPECT_NE(obj, nullptr);
}

TEST_F(BaseTest, throw_and_catch_exception) {
    auto clazz = find_class("java/lang/Exception");

    bool fired = false;
    try {
        throw_new_exception(clazz, "Oops");
    } catch (jvm_exception) {
        fired = true;
        EXPECT_NE(current_exception(), nullptr);
        clear_exception();
    }

    EXPECT_TRUE(fired);
    EXPECT_EQ(current_exception(), nullptr);
}

TEST_F(BaseTest, new_local_ref_then_delete_local_ref) {
    auto clazz = find_class("java/awt/Point");
    jobject obj1 = alloc_object(clazz);
    jobject obj2 = new_local_ref(obj1);
    EXPECT_NE(obj2, nullptr);
    EXPECT_TRUE(is_same_object(obj1, obj2));
    EXPECT_EQ(get_object_ref_type(obj2), JNILocalRefType);
    delete_local_ref(obj2);
}

TEST_F(BaseTest, new_global_ref_then_delete_global_ref) {
    auto clazz = find_class("java/awt/Point");
    jobject obj1 = alloc_object(clazz);
    jobject obj2 = new_global_ref(obj1);
    EXPECT_NE(obj2, nullptr);
    EXPECT_TRUE(is_same_object(obj1, obj2));
    EXPECT_EQ(get_object_ref_type(obj2), JNIGlobalRefType);
    delete_global_ref(obj2);
}

TEST_F(BaseTest, new_weak_ref_then_delete_weak_ref) {
    auto clazz = find_class("java/awt/Point");
    jobject obj1 = alloc_object(clazz);
    jobject obj2 = new_weak_global_ref(obj1);
    EXPECT_NE(obj2, nullptr);
    EXPECT_TRUE(is_same_object(obj1, obj2));
    EXPECT_EQ(get_object_ref_type(obj2), JNIWeakGlobalRefType);
    delete_weak_global_ref(obj2);
}

TEST_F(BaseTest, auto_refs_on_stack_are_local_refs) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject local_auto;
    new_auto_ref(&local_auto, obj);
    EXPECT_NE(local_auto, nullptr);
    EXPECT_TRUE(is_same_object(obj, local_auto));
    EXPECT_EQ(get_object_ref_type(local_auto), JNILocalRefType);
    delete_auto_ref(&local_auto);
}

TEST_F(BaseTest, auto_refs_on_heap_are_global_refs) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject* heap_auto = new jobject;
    new_auto_ref(heap_auto, obj);
    EXPECT_NE(*heap_auto, nullptr);
    EXPECT_TRUE(is_same_object(obj, *heap_auto));
    EXPECT_EQ(get_object_ref_type(*heap_auto), JNIGlobalRefType);
    delete_auto_ref(heap_auto);
    delete heap_auto;
}

TEST_F(BaseTest, moving_auto_ref_from_stack_to_stack_retains_local_ref) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject local_auto1;
    jobject local_auto2;
    new_auto_ref(&local_auto1, obj);
    jobject initial_auto = local_auto1;
    move_auto_ref(&local_auto2, &local_auto1);
    EXPECT_EQ(local_auto1, nullptr);
    EXPECT_NE(local_auto2, nullptr);
    EXPECT_EQ(local_auto2, initial_auto);
    EXPECT_TRUE(is_same_object(obj, local_auto2));
    EXPECT_EQ(get_object_ref_type(local_auto2), JNILocalRefType);
    delete_auto_ref(&local_auto1);
    delete_auto_ref(&local_auto2);
}

TEST_F(BaseTest, moving_auto_ref_from_heap_to_heap_retains_local_ref) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject* heap_auto1 = new jobject;
    jobject* heap_auto2 = new jobject;
    new_auto_ref(heap_auto1, obj);
    jobject initial_auto = *heap_auto1;
    move_auto_ref(heap_auto2, heap_auto1);
    EXPECT_EQ(*heap_auto1, nullptr);
    EXPECT_NE(*heap_auto2, nullptr);
    EXPECT_EQ(*heap_auto2, initial_auto);
    EXPECT_TRUE(is_same_object(obj, *heap_auto2));
    EXPECT_EQ(get_object_ref_type(*heap_auto2), JNIGlobalRefType);
    delete_auto_ref(heap_auto1);
    delete_auto_ref(heap_auto2);
    delete heap_auto1;
    delete heap_auto2;
}

TEST_F(BaseTest, moving_auto_ref_from_local_to_heap_deletes_local_and_creates_global) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject local_auto1;
    jobject* heap_auto2 = new jobject;
    new_auto_ref(&local_auto1, obj);
    jobject initial_auto = local_auto1;
    move_auto_ref(heap_auto2, &local_auto1);
    EXPECT_EQ(local_auto1, nullptr);
    EXPECT_NE(*heap_auto2, nullptr);
    EXPECT_NE(*heap_auto2, initial_auto);
    EXPECT_TRUE(is_same_object(obj, *heap_auto2));
    EXPECT_EQ(get_object_ref_type(*heap_auto2), JNIGlobalRefType);
    delete_auto_ref(&local_auto1);
    delete_auto_ref(heap_auto2);
    delete heap_auto2;
}

TEST_F(BaseTest, moving_auto_ref_from_heap_to_local_deletes_global_and_creates_local) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    jobject* heap_auto1 = new jobject;
    jobject local_auto2;
    new_auto_ref(heap_auto1, obj);
    jobject initial_auto = *heap_auto1;
    move_auto_ref(&local_auto2, heap_auto1);
    EXPECT_EQ(*heap_auto1, nullptr);
    EXPECT_NE(local_auto2, nullptr);
    EXPECT_NE(local_auto2, initial_auto);
    EXPECT_TRUE(is_same_object(obj, local_auto2));
    EXPECT_EQ(get_object_ref_type(local_auto2), JNILocalRefType);
    delete_auto_ref(heap_auto1);
    delete_auto_ref(&local_auto2);
    delete heap_auto1;
}

TEST_F(BaseTest, set_field_then_get_field) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    auto field_id = get_field_id(clazz, "x", "I");
    set_field(obj, field_id, jint(1));
    EXPECT_EQ(get_field<jint>(obj, field_id), 1);
}

TEST_F(BaseTest, get_static_field) {
    // Would like to test setting static fields too but I want this test to only depend on JDK in terms of Java classes
    // and I don't think the JDK contains any non-final public static fields.
    auto clazz = find_class("java/lang/System");
    auto field_id = get_static_field_id(clazz, "out", "Ljava/io/PrintStream;");
    EXPECT_TRUE(get_static_field<jobject>(clazz, field_id));
}

TEST_F(BaseTest, call_method) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);

    auto field_id = get_field_id(clazz, "x", "I");
    set_field(obj, field_id, jint(1));

    auto method_id = get_method_id(clazz, "getX", "()D");
    jdouble result = call_method<jdouble>(obj, method_id);

    EXPECT_EQ(result, 1.0);
}

TEST_F(BaseTest, call_nonvirtual_method) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);

    auto field_id = get_field_id(clazz, "x", "I");
    set_field(obj, field_id, jint(1));

    auto method_id = get_method_id(clazz, "getX", "()D");
    jdouble result = call_nonvirtual_method<jdouble>(obj, clazz, method_id);

    EXPECT_EQ(result, 1.0);
}

TEST_F(BaseTest, call_static_method) {
    auto clazz = find_class("java/lang/System");

    auto method_id = get_static_method_id(clazz, "nanoTime", "()J");
    jlong result = call_static_method<jlong>(clazz, method_id);

    EXPECT_NE(result, 0);
}

TEST_F(BaseTest, push_and_pop_local_frames_preserving_one_ref) {
    push_local_frame(1000);

    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    auto field_id = get_field_id(clazz, "x", "I");
    set_field(obj, field_id, jint(1));

    obj = pop_local_frame(obj);

    EXPECT_EQ(get_field<jint>(obj, field_id), 1);
}

TEST_F(BaseTest, get_hash_code_of_null_is_zero) {
    EXPECT_EQ(get_hash_code(nullptr), 0);
}

TEST_F(BaseTest, get_hash_code) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    EXPECT_EQ(get_hash_code(obj), get_hash_code(obj));
}

TEST_F(BaseTest, is_equal_object) {
    auto clazz = find_class("java/awt/Point");
    jobject obj1 = alloc_object(clazz);
    jobject obj2 = alloc_object(clazz);
    auto field_id = get_field_id(clazz, "x", "I");
    set_field(obj2, field_id, jint(1));

    EXPECT_TRUE(is_equal_object(obj1, obj1));
    EXPECT_FALSE(is_equal_object(obj1, obj2));
    EXPECT_FALSE(is_equal_object(nullptr, obj1));
    EXPECT_FALSE(is_equal_object(obj1, nullptr));
    EXPECT_TRUE(is_equal_object(nullptr, nullptr));
}

TEST_F(BaseTest, get_identity_hash_code_of_null_is_zero) {
    EXPECT_EQ(get_identity_hash_code(nullptr), 0);
}

TEST_F(BaseTest, get_identity_hash_code) {
    auto clazz = find_class("java/awt/Point");
    jobject obj = alloc_object(clazz);
    EXPECT_EQ(get_identity_hash_code(obj), get_identity_hash_code(obj));
}

TEST_F(BaseTest, string) {
    jstring str = new_string((const jchar*) u"Hello", 5);
    EXPECT_EQ(get_string_length(str), 5);

    const char16_t* chars = (const char16_t*) get_string_chars(str, nullptr);
    EXPECT_EQ(chars, std::u16string(u"Hello"));
    release_string_chars(str, (const jchar*) chars);
}

TEST_F(BaseTest, new_primitive_array) {
    jarray array = new_primitive_array<jint>(3);
    EXPECT_NE(array, nullptr);
}

TEST_F(BaseTest, get_array_length) {
    jarray array = new_primitive_array<jint>(3);
    EXPECT_EQ(get_array_length(array), 3);
}

TEST_F(BaseTest, set_then_get_array_elements) {
    jarray array = new_primitive_array<jint>(3);
    for (jint i = 0; i < 3; ++i) {
        set_array_element(array, i, i);
    }
    for (jint i = 0; i < 3; ++i) {
        EXPECT_EQ(get_array_element<jint>(array, i), i);
    }
}

TEST_F(BaseTest, get_and_release_array_elements) {
    jarray array = new_primitive_array<jint>(3);
    jboolean isCopy;
    jint* elements = get_array_elements<jint>(array, &isCopy);
    release_array_elements(array, elements, 0);
}

}  // namespace whatjni
