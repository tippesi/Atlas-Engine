#pragma once

#include <string>

namespace Helper {

    namespace String {

        // Case sensitive match with the wildcards '*' (any sequence) and '?' (any character)
        inline bool MatchWildcard(const std::string& pattern, const std::string& text) {

            // Without a star every pattern character matches exactly one character
            if (pattern.size() != text.size() && pattern.find('*') == std::string::npos)
                return false;

            // Backtracks to the last star when a character doesn't match
            size_t p = 0, t = 0, star = std::string::npos, starMatch = 0;
            while (t < text.size()) {
                if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
                    p++;
                    t++;
                }
                else if (p < pattern.size() && pattern[p] == '*') {
                    star = p++;
                    starMatch = t;
                }
                else if (star != std::string::npos) {
                    p = star + 1;
                    t = ++starMatch;
                }
                else {
                    return false;
                }
            }

            while (p < pattern.size() && pattern[p] == '*')
                p++;

            return p == pattern.size();

        }

    }

}
