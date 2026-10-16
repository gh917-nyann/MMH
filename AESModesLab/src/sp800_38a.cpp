#include "sp800_38a.hpp"
namespace lab
{
    Bytes ecb_encrypt(const AES &aes, const Bytes &in, bool padding)
    {
        Bytes d = padding ? pad(in) : in;
        check_aligned(d);
        Bytes out;
        for (size_t i = 0; i < d.size(); i += 16)
            append(out, aes.encrypt(block(d, i)));
        return out;
    }
    Bytes ecb_decrypt(const AES &aes, const Bytes &d, bool padding)
    {
        check_aligned(d);
        Bytes out;
        for (size_t i = 0; i < d.size(); i += 16)
            append(out, aes.decrypt(block(d, i)));
        return padding ? unpad(out) : out;
    }
    Bytes cbc_encrypt(const AES &aes, const Bytes &in, const Bytes &iv, bool padding)
    {
        check_iv(iv);
        Bytes d = padding ? pad(in) : in;
        check_aligned(d);
        Bytes out;
        Block prev = block(iv);
        for (size_t i = 0; i < d.size(); i += 16)
        {
            prev = aes.encrypt(xorblock(block(d, i), prev));
            append(out, prev);
        }
        return out;
    }
    Bytes cbc_decrypt(const AES &aes, const Bytes &d, const Bytes &iv, bool padding)
    {
        check_iv(iv);
        check_aligned(d);
        Bytes out;
        Block prev = block(iv);
        for (size_t i = 0; i < d.size(); i += 16)
        {
            Block cur = block(d, i);
            append(out, xorblock(aes.decrypt(cur), prev));
            prev = cur;
        }
        return padding ? unpad(out) : out;
    }
    static Bytes cfb(const AES &aes, const Bytes &d, const Bytes &iv, int segbits, bool encrypt)
    {
        check_iv(iv);
        if (segbits < 8 || segbits > 128 || segbits % 8)
            throw std::invalid_argument("CFB segment bits must be 8..128 byte aligned");
        size_t n = segbits / 8;
        Block reg = block(iv);
        Bytes out;
        for (size_t i = 0; i < d.size(); i += n)
        {
            auto stream = aes.encrypt(reg);
            size_t take = std::min(n, d.size() - i);
            for (size_t j = 0; j < take; j++)
                out.push_back(d[i + j] ^ stream[j]);
            if (take == n)
            {
                for (size_t j = 0; j < 16 - n; j++)
                    reg[j] = reg[j + n];
                for (size_t j = 0; j < n; j++)
                    reg[16 - n + j] = encrypt ? out[i + j] : d[i + j];
            }
        }
        return out;
    }
    Bytes cfb_encrypt(const AES &a, const Bytes &d, const Bytes &iv, int n) { return cfb(a, d, iv, n, true); }
    Bytes cfb_decrypt(const AES &a, const Bytes &d, const Bytes &iv, int n) { return cfb(a, d, iv, n, false); }
    Bytes ofb_crypt(const AES &aes, const Bytes &d, const Bytes &iv)
    {
        check_iv(iv);
        Block feed = block(iv);
        Bytes out;
        for (size_t i = 0; i < d.size(); i += 16)
        {
            feed = aes.encrypt(feed);
            for (size_t j = 0; j < std::min(size_t(16), d.size() - i); j++)
                out.push_back(d[i + j] ^ feed[j]);
        }
        return out;
    }
    Bytes ctr_crypt(const AES &aes, const Bytes &d, const Bytes &iv)
    {
        check_iv(iv);
        Block counter = block(iv);
        Bytes out;
        for (size_t i = 0; i < d.size(); i += 16)
        {
            auto stream = aes.encrypt(counter);
            for (size_t j = 0; j < std::min(size_t(16), d.size() - i); j++)
                out.push_back(d[i + j] ^ stream[j]);
            if (i + 16 < d.size())
            {
                bool wrapped = true;
                for (int j = 15; j >= 0; j--)
                {
                    if (++counter[j] != 0)
                    {
                        wrapped = false;
                        break;
                    }
                }
                if (wrapped)
                    throw std::overflow_error("CTR counter wrapped");
            }
        }
        return out;
    }
}
