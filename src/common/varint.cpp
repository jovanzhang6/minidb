/**
 * @file varint.cpp
 * @brief Variable-length integer encoding/decoding implementation
 */

#include "varint.h"

namespace minidb {

int Varint::Encode(uint64_t value, uint8_t* buffer) {
    // SQLite-style varint: big-endian, 7 bits per byte, high bit is continuation flag
    if (value <= 0x7F) {
        buffer[0] = static_cast<uint8_t>(value);
        return 1;
    }

    // Count how many bytes we need
    uint8_t temp[9];
    int n = 0;
    
    // Build from least significant to most significant
    while (value > 0x7F) {
        temp[n++] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
    }
    temp[n++] = static_cast<uint8_t>(value);

    // Write in reverse order (big-endian) with continuation bits
    for (int i = 0; i < n; ++i) {
        if (i < n - 1) {
            // Not the last byte, set continuation bit
            buffer[i] = temp[n - 1 - i] | 0x80;
        } else {
            // Last byte, no continuation bit
            buffer[i] = temp[n - 1 - i];
        }
    }

    return n;
}

int Varint::Decode(const uint8_t* buffer, uint64_t* value) {
    uint64_t result = 0;
    int i = 0;
    
    // Read bytes until we find one without continuation bit
    for (i = 0; i < MAX_VARINT_LENGTH; ++i) {
        uint8_t byte = buffer[i];
        
        if (i == MAX_VARINT_LENGTH - 1) {
            // 9th byte: all 8 bits are data
            result = (result << 8) | byte;
            i++;
            break;
        }
        
        // Normal byte: 7 bits of data
        result = (result << 7) | (byte & 0x7F);
        
        if ((byte & 0x80) == 0) {
            // No continuation bit, we're done
            i++;
            break;
        }
    }
    
    *value = result;
    return i;
}

int Varint::EncodeSigned(int64_t value, uint8_t* buffer) {
    // Treat as unsigned for encoding
    return Encode(static_cast<uint64_t>(value), buffer);
}

int Varint::DecodeSigned(const uint8_t* buffer, int64_t* value) {
    uint64_t unsigned_val;
    int bytes = Decode(buffer, &unsigned_val);
    *value = static_cast<int64_t>(unsigned_val);
    return bytes;
}

int Varint::EncodedLength(uint64_t value) {
    if (value <= 0x7F) return 1;
    if (value <= 0x3FFF) return 2;
    if (value <= 0x1FFFFF) return 3;
    if (value <= 0x0FFFFFFF) return 4;
    if (value <= 0x07FFFFFFFF) return 5;
    if (value <= 0x03FFFFFFFFFF) return 6;
    if (value <= 0x01FFFFFFFFFFFF) return 7;
    if (value <= 0x00FFFFFFFFFFFFFF) return 8;
    return 9;
}

int Varint::DecodedLength(uint8_t first_byte) {
    // Count leading 1 bits to determine length
    if ((first_byte & 0x80) == 0) return 1;
    if ((first_byte & 0xC0) == 0x80) return 2;
    if ((first_byte & 0xE0) == 0xC0) return 3;
    if ((first_byte & 0xF0) == 0xE0) return 4;
    if ((first_byte & 0xF8) == 0xF0) return 5;
    if ((first_byte & 0xFC) == 0xF8) return 6;
    if ((first_byte & 0xFE) == 0xFC) return 7;
    if (first_byte == 0xFE) return 8;
    return 9;  // 0xFF
}

} // namespace minidb
