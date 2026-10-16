#include "sp800_38c.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace lab {
namespace {
constexpr size_t BLOCK_SIZE = 16;

bool valid_tag_length(size_t n) {
    return n >= 4 && n <= 16 && (n % 2 == 0);
}

void validate_inputs(const Bytes& nonce, size_t tag_length) {
    if (nonce.size() < 7 || nonce.size() > 13)
        throw std::invalid_argument("CCM nonce length must be between 7 and 13 bytes");
    if (!valid_tag_length(tag_length))
        throw std::invalid_argument("CCM tag length must be 4, 6, 8, 10, 12, 14, or 16 bytes");
}

Block xor_block(const Block& a, const Block& b) {
    Block out{};
    for (size_t i = 0; i < BLOCK_SIZE; ++i) out[i] = a[i] ^ b[i];
    return out;
}

void put_length(Block& block, size_t offset, size_t length, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) {
        block[offset + bytes - 1 - i] = static_cast<uint8_t>(length & 0xffU);
        length >>= 8;
    }
}

void append_length(Bytes& out, uint64_t length, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) {
        out.push_back(static_cast<uint8_t>((length >> (8 * (bytes - 1 - i))) & 0xffU));
    }
}

// CCM encodes the associated-data length before the associated data itself.

Bytes encode_aad(const Bytes& aad) {
    Bytes encoded;

    if (aad.empty()) {
        return encoded;
    }

    if (aad.size() < 0xff00U) {
        append_length(encoded, static_cast<uint64_t>(aad.size()), 2);
    } else if (aad.size() <= 0xffffffffULL) {
        encoded.push_back(0xff);
        encoded.push_back(0xfe);
        append_length(encoded, static_cast<uint64_t>(aad.size()), 4);
    } else {
        encoded.push_back(0xff);
        encoded.push_back(0xff);
        append_length(encoded, static_cast<uint64_t>(aad.size()), 8);
    }

    encoded.reserve(encoded.size() + aad.size());

    for (const auto byte : aad) {
        encoded.push_back(byte);
    }

    return encoded;
}


void mac_block(const AES& aes, Block& state, const Block& input) {
    state = aes.encrypt(xor_block(state, input));
}

Block calculate_mac(const AES& aes, const Bytes& plaintext, const Bytes& nonce,
                    const Bytes& aad, size_t tag_length, size_t length_bytes) {
    Block state{};
    Block b0{};
    const bool has_aad = !aad.empty();
    b0[0] = static_cast<uint8_t>((has_aad ? 0x40 : 0) |
                                 (((tag_length - 2) / 2) << 3) |
                                 (length_bytes - 1));
    std::copy(nonce.begin(), nonce.end(), b0.begin() + 1);
    put_length(b0, BLOCK_SIZE - length_bytes, plaintext.size(), length_bytes);
    mac_block(aes, state, b0);

    Bytes encoded_aad = encode_aad(aad);
    for (size_t offset = 0; offset < encoded_aad.size(); offset += BLOCK_SIZE) {
        Block block{};
        const size_t take = std::min(BLOCK_SIZE, encoded_aad.size() - offset);
        std::copy_n(encoded_aad.begin() + static_cast<std::ptrdiff_t>(offset), take, block.begin());
        mac_block(aes, state, block);
    }

    for (size_t offset = 0; offset < plaintext.size(); offset += BLOCK_SIZE) {
        Block block{};
        const size_t take = std::min(BLOCK_SIZE, plaintext.size() - offset);
        std::copy_n(plaintext.begin() + static_cast<std::ptrdiff_t>(offset), take, block.begin());
        mac_block(aes, state, block);
    }
    return state;
}

Block counter_block(const Bytes& nonce, size_t length_bytes, uint64_t counter) {
    Block block{};
    block[0] = static_cast<uint8_t>(length_bytes - 1);
    std::copy(nonce.begin(), nonce.end(), block.begin() + 1);
    for (size_t i = 0; i < length_bytes; ++i) {
        block[BLOCK_SIZE - 1 - i] = static_cast<uint8_t>(counter & 0xffU);
        counter >>= 8;
    }
    return block;
}

Bytes crypt_ctr(const AES& aes, const Bytes& input, const Bytes& nonce, size_t length_bytes) {
    Bytes out;
    out.reserve(input.size());
    uint64_t counter = 1;
    for (size_t offset = 0; offset < input.size(); offset += BLOCK_SIZE, ++counter) {
        const Block stream = aes.encrypt(counter_block(nonce, length_bytes, counter));
        const size_t take = std::min(BLOCK_SIZE, input.size() - offset);
        for (size_t j = 0; j < take; ++j) out.push_back(input[offset + j] ^ stream[j]);
    }
    return out;
}
} // namespace

Bytes ccm_encrypt(const AES& aes, const Bytes& plaintext, const Bytes& nonce,
                  const Bytes& aad, size_t tag_length) {
    validate_inputs(nonce, tag_length);
    const size_t length_bytes = 15 - nonce.size();
    if (length_bytes < sizeof(size_t) && plaintext.size() >= (size_t(1) << (8 * length_bytes)))
        throw std::length_error("CCM plaintext is too long for the selected nonce length");

    const Block mac = calculate_mac(aes, plaintext, nonce, aad, tag_length, length_bytes);
    const Block s0 = aes.encrypt(counter_block(nonce, length_bytes, 0));
    Bytes ciphertext = crypt_ctr(aes, plaintext, nonce, length_bytes);
    ciphertext.reserve(ciphertext.size() + tag_length);
    for (size_t i = 0; i < tag_length; ++i) ciphertext.push_back(mac[i] ^ s0[i]);
    return ciphertext;
}

Bytes ccm_decrypt(const AES& aes, const Bytes& input, const Bytes& nonce,
                  const Bytes& aad, size_t tag_length) {
    validate_inputs(nonce, tag_length);
    if (input.size() < tag_length)
        throw std::invalid_argument("CCM input is shorter than the authentication tag");

    const size_t length_bytes = 15 - nonce.size();
    Bytes ciphertext(input.begin(), input.end() - static_cast<std::ptrdiff_t>(tag_length));
    Bytes received_tag(input.end() - static_cast<std::ptrdiff_t>(tag_length), input.end());
    if (length_bytes < sizeof(size_t) && ciphertext.size() >= (size_t(1) << (8 * length_bytes)))
        throw std::length_error("CCM ciphertext is too long for the selected nonce length");

    Bytes plaintext = crypt_ctr(aes, ciphertext, nonce, length_bytes);
    const Block mac = calculate_mac(aes, plaintext, nonce, aad, tag_length, length_bytes);
    const Block s0 = aes.encrypt(counter_block(nonce, length_bytes, 0));

    uint8_t difference = 0;
    for (size_t i = 0; i < tag_length; ++i)
        difference |= static_cast<uint8_t>(received_tag[i] ^ mac[i] ^ s0[i]);
    if (difference != 0)
        throw std::invalid_argument("CCM authentication failed");
    return plaintext;
}
} // namespace lab
