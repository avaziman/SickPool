
#define CompareSpans(a, b)         \
    ASSERT_EQ(a.size(), b.size()); \
    ASSERT_FALSE(memcmp(a.data(), b.data(), a.size()))
