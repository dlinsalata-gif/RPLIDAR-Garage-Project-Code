#pragma once
#include <unordered_set>
#include <string>
#include <algorithm>
#include <cctype>

static std::unordered_set<std::string> teacher_tags = {
    "E2000017221101441890ABCD",
    "E2000017221101441890EFGH"
};

inline std::string normalize_tag(std::string tag) {
    tag.erase(
        std::remove_if(tag.begin(), tag.end(),
            [](unsigned char c) { return std::isspace(c); }),
        tag.end()
    );

    std::transform(tag.begin(), tag.end(), tag.begin(),
        [](unsigned char c) { return std::toupper(c); });

    return tag;
}

inline bool isTeacherTag(const std::string& tag) {
    return teacher_tags.count(normalize_tag(tag)) > 0;
}
