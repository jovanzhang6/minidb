/**
 * @file varint.h
 * @brief Variable-length integer encoding/decoding (SQLite compatible)
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace minidb {

/**
 * @brief Variable-length integer utilities
 * 
 * SQLite-style varint encoding:
 * - Uses 1-9 bytes to encode 64-bit integers
 * - Each byte's high bit (bit 7) indicates continuation
 * - Remaining 7 bits are data
 * - Values are stored in big-endian order
 */
class Varint {
public:
    /**
     * @brief Encode a 64-bit unsigned integer as varint
     * @param value The value to encode
     * @param buffer Output buffer (must be at least 9 bytes)
     * @return Number of bytes written
     */
    static int Encode(uint64_t value, uint8_t* buffer);

    /**
     * @brief Decode a varint to 64-bit unsigned integer
     * @param buffer Input buffer
     * @param value Output value
     * @return Number of bytes read
     */
    static int Decode(const uint8_t* buffer, uint64_t* value);

    /**
     * @brief Encode a 64-bit signed integer as varint
     * @param value The value to encode
     * @param buffer Output buffer (must be at least 9 bytes)
     * @return Number of bytes written
     */
    static int EncodeSigned(int64_t value, uint8_t* buffer);

    /**
     * @brief Decode a varint to 64-bit signed integer
     * @param buffer Input buffer
     * @param value Output value
     * @return Number of bytes read
     */
    static int DecodeSigned(const uint8_t* buffer, int64_t* value);

    /**
     * @brief Calculate bytes needed to encode a value
     * @param value The value to check
     * @return Number of bytes needed (1-9)
     */
    static int EncodedLength(uint64_t value);

    /**
     * @brief Get the length of a varint from its first byte
     * @param first_byte The first byte of the varint
     * @return Number of bytes in this varint
     */
    static int DecodedLength(uint8_t first_byte);

    // Maximum bytes for a varint
    static constexpr int MAX_VARINT_LENGTH = 9;
};

} // namespace minidb
