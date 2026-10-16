#pragma once

#include "aes.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lab {

using Byte = AES::Byte;
using Block = AES::Block;
using Bytes = std::vector<Byte>;

inline constexpr std::size_t BLOCK_SIZE = AES::BLOCK_SIZE;

inline void append(Bytes& destination, const Block& block_value) {
    destination.insert(destination.end(), block_value.begin(), block_value.end());
}

inline Block block(const Bytes& input, std::size_t offset = 0) {
    if (offset > input.size() || input.size() - offset < BLOCK_SIZE) {
        throw std::invalid_argument("Input does not contain a complete AES block at the requested offset");
    }

    Block result{};
    std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(offset), BLOCK_SIZE, result.begin());
    return result;
}

inline Block xorblock(const Block& left, const Block& right) {
    Block result{};
    for (std::size_t i = 0; i < BLOCK_SIZE; ++i) {
        result[i] = static_cast<Byte>(left[i] ^ right[i]);
    }
    return result;
}

inline void check_iv(const Bytes& iv) {
    if (iv.size() != BLOCK_SIZE) {
        throw std::invalid_argument("AES IV/counter must be exactly 16 bytes");
    }
}

inline void check_aligned(const Bytes& input) {
    if (input.size() % BLOCK_SIZE != 0) {
        throw std::invalid_argument("Input length must be a multiple of 16 bytes");
    }
}

// PKCS#7 padding, including a full padding block for already aligned input.
inline Bytes pad(const Bytes& input) {
    const auto padding_size = BLOCK_SIZE - (input.size() % BLOCK_SIZE);
    Bytes result = input;
    result.insert(result.end(), padding_size, static_cast<Byte>(padding_size));
    return result;
}

inline Bytes unpad(const Bytes& input) {
    check_aligned(input);
    if (input.empty()) {
        throw std::invalid_argument("Cannot remove PKCS#7 padding from empty input");
    }

    const auto padding_size = static_cast<std::size_t>(input.back());
    if (padding_size == 0 || padding_size > BLOCK_SIZE || padding_size > input.size()) {
        throw std::invalid_argument("Invalid PKCS#7 padding");
    }
    const auto padding_begin = input.end() - static_cast<std::ptrdiff_t>(padding_size);
    if (!std::all_of(padding_begin, input.end(), [padding_size](Byte value) {
            return value == static_cast<Byte>(padding_size);
        })) {
        throw std::invalid_argument("Invalid PKCS#7 padding");
    }

    return Bytes(input.begin(), padding_begin);
}

} // namespace lab
