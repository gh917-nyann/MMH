#include "aes.hpp"
#include "common.hpp"
#include "sp800_38a.hpp"
#include "sp800_38c.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using lab::Bytes;

Bytes fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::invalid_argument("Hex string must have even length");
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("Invalid hex character");
    };
    Bytes out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
        out.push_back(static_cast<AES::Byte>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    return out;
}

std::string toHex(const Bytes& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto b : bytes) {
        out.push_back(digits[(b >> 4) & 0x0f]);
        out.push_back(digits[b & 0x0f]);
    }
    return out;
}

struct TestRunner {
    int passed = 0;
    int failed = 0;

    template<class F>
    void run(const char* name, F&& test) {
        try {
            if (test()) {
                std::cout << "PASS " << name << '\n';
                ++passed;
            } else {
                std::cout << "FAIL " << name << '\n';
                ++failed;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL " << name << " (" << e.what() << ")\n";
            ++failed;
        } catch (...) {
            std::cout << "FAIL " << name << " (unknown exception)\n";
            ++failed;
        }
    }
};

bool aes128_fips197() {
    const AES aes(fromHex("000102030405060708090a0b0c0d0e0f"));
    const Bytes plaintext = fromHex("00112233445566778899aabbccddeeff");
    const Bytes expected = fromHex("69c4e0d86a7b0430d8cdb78070b4c55a");
    AES::Block block{};
    std::copy(plaintext.begin(), plaintext.end(), block.begin());
    const auto encrypted = aes.encrypt(block);
    if (!std::equal(encrypted.begin(), encrypted.end(), expected.begin())) return false;
    const auto decrypted = aes.decrypt(encrypted);
    return std::equal(decrypted.begin(), decrypted.end(), plaintext.begin());
}

} // namespace

int main() {
    TestRunner tests;

    // AES-128 test key and the NIST SP 800-38A Appendix F plaintext.
    const AES aes(fromHex("2b7e151628aed2a6abf7158809cf4f3c"));
    const Bytes plaintext = fromHex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52ef"
        "f69f2445df4f9b17ad2b417be66c3710");
    const Bytes iv = fromHex("000102030405060708090a0b0c0d0e0f");
    const Bytes ctr = fromHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

    const Bytes ecb_expected = fromHex(
        "3ad77bb40d7a3660a89ecaf32466ef97"
        "f5d3d58503b9699de785895a96fdbaaf"
        "43b1cd7f598ece23881b00e3ed030688"
        "7b0c785e27e8ad3f8223207104725dd4");
    const Bytes cbc_expected = fromHex(
        "7649abac8119b246cee98e9b12e9197d"
        "5086cb9b507219ee95db113a917678b2"
        "73bed6b8e3c1743b7116e69e22229516"
        "3ff1caa1681fac09120eca307586e1a7");
    const Bytes cfb128_expected = fromHex(
        "3b3fd92eb72dad20333449f8e83cfb4a"
        "c8a64537a0b3a93fcde3cdad9f1ce58b"
        "26751f67a3cbb140b1808cf187a4f4df"
        "c04b05357c5d1c0eeac4c66f9ff7f2e6");
    const Bytes cfb8_plaintext = fromHex("6bc1bee22e409f96e93d7e117393172aae2d");
    const Bytes cfb8_expected = fromHex("3b79424c9c0dd436bace9e0ed4586a4f32b9");
    const Bytes ofb_expected = fromHex(
        "3b3fd92eb72dad20333449f8e83cfb4a"
        "7789508d16918f03f53c52dac54ed825"
        "9740051e9c5fecf64344f7a82260edcc"
        "304c6528f659c77866a510d9c1d6ae5e");
    const Bytes ctr_expected = fromHex(
        "874d6191b620e3261bef6864990db6ce"
        "9806f66b7970fdff8617187bb9fffdff"
        "5ae4df3edbd5d35e5b4f09020db03eab"
        "1e031dda2fbe03d1792170a0f3009cee");

    tests.run("AES-128 FIPS-197", [] { return aes128_fips197(); });
    tests.run("ECB", [&] {
        return lab::ecb_encrypt(aes, plaintext, false) == ecb_expected &&
               lab::ecb_decrypt(aes, ecb_expected, false) == plaintext;
    });
    tests.run("CBC", [&] {
        return lab::cbc_encrypt(aes, plaintext, iv, false) == cbc_expected &&
               lab::cbc_decrypt(aes, cbc_expected, iv, false) == plaintext;
    });
    tests.run("CFB128", [&] {
        return lab::cfb_encrypt(aes, plaintext, iv, 128) == cfb128_expected &&
               lab::cfb_decrypt(aes, cfb128_expected, iv, 128) == plaintext;
    });
    tests.run("CFB8", [&] {
        return lab::cfb_encrypt(aes, cfb8_plaintext, iv, 8) == cfb8_expected &&
               lab::cfb_decrypt(aes, cfb8_expected, iv, 8) == cfb8_plaintext;
    });
    tests.run("OFB", [&] {
        return lab::ofb_crypt(aes, plaintext, iv) == ofb_expected &&
               lab::ofb_crypt(aes, ofb_expected, iv) == plaintext;
    });
    tests.run("CTR", [&] {
        return lab::ctr_crypt(aes, plaintext, ctr) == ctr_expected &&
               lab::ctr_crypt(aes, ctr_expected, ctr) == plaintext;
    });

    // RFC 3610, Packet Vector #1 (CCM; ciphertext is followed by an 8-byte tag).
    const AES ccm_aes(fromHex("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"));
    const Bytes nonce = fromHex("00000003020100a0a1a2a3a4a5");
    const Bytes aad = fromHex("0001020304050607");
    const Bytes ccm_plaintext = fromHex("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e");
    const Bytes ccm_expected = fromHex(
        "588c979a61c663d2f066d0c2c0f98980"
        "6d5f6b61dac38417e8d12cfdf926e0");
    const Bytes ccm_actual = lab::ccm_encrypt(ccm_aes, ccm_plaintext, nonce, aad, 8);
    const Bytes expected_ciphertext(ccm_expected.begin(), ccm_expected.end() - 8);
    const Bytes expected_tag(ccm_expected.end() - 8, ccm_expected.end());
    const Bytes actual_ciphertext(ccm_actual.begin(), ccm_actual.end() - 8);
    const Bytes actual_tag(ccm_actual.end() - 8, ccm_actual.end());

    tests.run("CCM RFC3610 ct", [&] { return actual_ciphertext == expected_ciphertext; });
    tests.run("CCM RFC3610 tag", [&] { return actual_tag == expected_tag; });
    tests.run("CCM RFC3610 decrypt/auth", [&] {
        return lab::ccm_decrypt(ccm_aes, ccm_expected, nonce, aad, 8) == ccm_plaintext;
    });

    std::cout << "\n";
    if (tests.failed == 0) {
        std::cout << "ALL " << tests.passed << " VECTORS PASS\n";
        return 0;
    }
    std::cout << tests.failed << " TEST(S) FAILED; " << tests.passed << "/"
              << (tests.passed + tests.failed) << " PASSED\n";
    return 1;
}
