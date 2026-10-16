#pragma once
#include "common.hpp"
#include <cstddef>

namespace lab
{
    // CCM returns ciphertext || authentication tag. The nonce is supplied separately.
    // Valid nonce lengths: 7..13 bytes. Valid tag lengths: 4, 6, 8, 10, 12, 14, or 16 bytes.
    Bytes ccm_encrypt(const AES &aes, const Bytes &plaintext, const Bytes &nonce,
                      const Bytes &aad = Bytes{}, size_t tag_length = 16);

    // Input is ciphertext || authentication tag. Throws std::invalid_argument if authentication fails.
    Bytes ccm_decrypt(const AES &aes, const Bytes &ciphertext_and_tag, const Bytes &nonce,
                      const Bytes &aad = Bytes{}, size_t tag_length = 16);
}
