#include "whatjni/local_frame.h"
#include "whatjni/string.h"

#include "gtest/gtest.h"

#include <sstream>

namespace whatjni {

struct StringTest: testing::Test {
    local_frame frame;

    jstring to_jstring(java::lang::String* str) {
        return reinterpret_cast<jstring>(str);
    }

    bool equals(java::lang::String* jstr, const char* expected) {
        jsize size = get_string_multibyte_size(reinterpret_cast<jstring>(jstr));
        if (size != strlen(expected)) {
            return false;
        }

        const char* chars = get_string_multibyte_chars(reinterpret_cast<jstring>(jstr), nullptr);
        bool result = memcmp(chars, expected, size) == 0;
        release_string_multibyte_chars(reinterpret_cast<jstring>(jstr), chars);
        return result;
    }
};

TEST_F(StringTest, double_rabbit_ear_operator) {
    EXPECT_TRUE(equals("Hello"_j, "Hello"));
}

TEST_F(StringTest, from_char_pointer) {
    EXPECT_TRUE(equals(j_string("Hello"), "Hello"));
}

TEST_F(StringTest, from_null_terminated_char16_pointer) {
    EXPECT_TRUE(equals(j_string(u"Hello", 5), "Hello"));
}

TEST_F(StringTest, from_char16_pointer_with_length) {
    EXPECT_TRUE(equals(j_string(u"Hello", 5), "Hello"));
}

TEST_F(StringTest, from_std_string) {
    EXPECT_TRUE(equals(j_string(std::string("Hello")), "Hello"));
}

TEST_F(StringTest, from_std_u16string) {
    EXPECT_TRUE(equals(j_string(std::u16string(u"Hello")), "Hello"));
}

TEST_F(StringTest, to_std_string) {
    EXPECT_EQ(std::string("Hello"), std_string("Hello"_j));
}

TEST_F(StringTest, to_std_u16string) {
    EXPECT_EQ(std::u16string(u"Hello"), std_u16string("Hello"_j));
}

TEST_F(StringTest, stream) {
    std::stringstream stream;
    stream << "Hello "_j << static_cast<java::lang::Object*>(nullptr);
    EXPECT_EQ(stream.str(), "Hello null");
}

}  // namespace whatjni
