// Detects where a streamed JSON object or array ends, without parsing it.
//
// The price API answers without Content-Length, so the end of the body is
// normally signalled only by the connection closing. On a weak link that close
// can arrive late or get lost, which would waste a complete response.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gold {

class JsonEndDetector {
public:
    // Returns true once the outermost object or array has been closed.
    bool feed(const char* data, size_t length) {
        for (size_t i = 0; i < length && !complete_; ++i) {
            const char c = data[i];
            if (inString_) {
                if (escaped_) {
                    escaped_ = false;
                } else if (c == '\\') {
                    escaped_ = true;
                } else if (c == '"') {
                    inString_ = false;
                }
            } else if (c == '"') {
                inString_ = true;
            } else if (c == '{' || c == '[') {
                if (depth_ < UINT16_MAX) ++depth_;
            } else if ((c == '}' || c == ']') && depth_ > 0) {
                if (--depth_ == 0) complete_ = true;
            }
        }
        return complete_;
    }

    bool complete() const { return complete_; }

private:
    uint16_t depth_ = 0;
    bool inString_ = false;
    bool escaped_ = false;
    bool complete_ = false;
};

}  // namespace gold
