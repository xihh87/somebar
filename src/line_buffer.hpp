// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <array>
#include <string.h>

// reads data from Reader, and passes complete lines to Handler.
template<size_t N>
class LineBuffer {
    std::array<char, N> _buffer;
    size_t _bytesBuffered {0};
    size_t _bytesConsumed {0};
    bool _discardLine {0};
public:
    template<typename Reader, typename Handler>
    size_t readLines(const Reader& reader, const Handler& handler)
    {
        auto d = _buffer.data();
        while (true) {
            auto bytesRead = reader(d + _bytesBuffered, _buffer.size() - _bytesBuffered);
            if (bytesRead <= 0) {
                return bytesRead;
            }

            _bytesBuffered += bytesRead;
            char* linePosition = nullptr;
            do {
                char* lineStart = d + _bytesConsumed;
                linePosition = static_cast<char*>(memchr(
                    lineStart,
                    '\n',
                    _bytesBuffered - _bytesConsumed));

                if (linePosition) {
                    int lineLength = linePosition - lineStart;
                    if (!_discardLine) {
                        handler(lineStart, lineLength);
                    }
                    _bytesConsumed += lineLength + 1;
                    _discardLine = false;
                }
            } while (linePosition);

            size_t bytesRemaining = _bytesBuffered - _bytesConsumed;
            if (bytesRemaining == _buffer.size()) {
                // line too long
                _discardLine = true;
                _bytesBuffered = 0;
                _bytesConsumed = 0;
            } else if (bytesRemaining > 0 && _bytesConsumed > 0) {
                // move the last partial message to the front of the buffer, so a full-sized
                // message will fit
                memmove(d, d+_bytesConsumed, bytesRemaining);
                _bytesBuffered = bytesRemaining;
                _bytesConsumed = 0;
            }
        }
    }
};
