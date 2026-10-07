#include "../fvm.hpp"
#include "util.hpp"

namespace fairuz::runtime {

namespace {

constexpr u32 rotate_right(u32 value, u32 count)
{
    return (value >> count) | (value << (32 - count));
}

class Sha256 {
public:
    void update(std::string_view input)
    {
        m_bit_count += static_cast<u64>(input.size()) * 8;
        for (unsigned char byte : input) {
            m_buffer[m_buffer_size++] = byte;
            if (m_buffer_size == m_buffer.size()) {
                transform(m_buffer.data());
                m_buffer_size = 0;
            }
        }
    }

    std::array<u8, 32> finish()
    {
        u64 original_bit_count = m_bit_count;
        m_buffer[m_buffer_size++] = 0x80;
        if (m_buffer_size > 56) {
            std::fill(m_buffer.begin() + static_cast<ptrdiff_t>(m_buffer_size), m_buffer.end(), 0);
            transform(m_buffer.data());
            m_buffer_size = 0;
        }
        std::fill(m_buffer.begin() + static_cast<ptrdiff_t>(m_buffer_size), m_buffer.begin() + 56, 0);
        for (int i = 0; i < 8; i++)
            m_buffer[63 - i] = static_cast<u8>(original_bit_count >> (i * 8));
        transform(m_buffer.data());

        std::array<u8, 32> digest { };
        for (size_t i = 0; i < m_state.size(); i++) {
            digest[i * 4] = static_cast<u8>(m_state[i] >> 24);
            digest[i * 4 + 1] = static_cast<u8>(m_state[i] >> 16);
            digest[i * 4 + 2] = static_cast<u8>(m_state[i] >> 8);
            digest[i * 4 + 3] = static_cast<u8>(m_state[i]);
        }
        return digest;
    }

private:
    void transform(u8 const* block)
    {
        static constexpr std::array<u32, 64> constants {
            0x428a2f98,
            0x71374491,
            0xb5c0fbcf,
            0xe9b5dba5,
            0x3956c25b,
            0x59f111f1,
            0x923f82a4,
            0xab1c5ed5,
            0xd807aa98,
            0x12835b01,
            0x243185be,
            0x550c7dc3,
            0x72be5d74,
            0x80deb1fe,
            0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1,
            0xefbe4786,
            0x0fc19dc6,
            0x240ca1cc,
            0x2de92c6f,
            0x4a7484aa,
            0x5cb0a9dc,
            0x76f988da,
            0x983e5152,
            0xa831c66d,
            0xb00327c8,
            0xbf597fc7,
            0xc6e00bf3,
            0xd5a79147,
            0x06ca6351,
            0x14292967,
            0x27b70a85,
            0x2e1b2138,
            0x4d2c6dfc,
            0x53380d13,
            0x650a7354,
            0x766a0abb,
            0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1,
            0xa81a664b,
            0xc24b8b70,
            0xc76c51a3,
            0xd192e819,
            0xd6990624,
            0xf40e3585,
            0x106aa070,
            0x19a4c116,
            0x1e376c08,
            0x2748774c,
            0x34b0bcb5,
            0x391c0cb3,
            0x4ed8aa4a,
            0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee,
            0x78a5636f,
            0x84c87814,
            0x8cc70208,
            0x90befffa,
            0xa4506ceb,
            0xbef9a3f7,
            0xc67178f2,
        };

        std::array<u32, 64> words { };
        for (size_t i = 0; i < 16; i++) {
            size_t offset = i * 4;
            words[i] = (static_cast<u32>(block[offset]) << 24)
                | (static_cast<u32>(block[offset + 1]) << 16)
                | (static_cast<u32>(block[offset + 2]) << 8)
                | static_cast<u32>(block[offset + 3]);
        }
        for (size_t i = 16; i < words.size(); i++) {
            u32 s0 = rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
            u32 s1 = rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        u32 a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        u32 e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
        for (size_t i = 0; i < words.size(); i++) {
            u32 sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            u32 choice = (e & f) ^ (~e & g);
            u32 temp1 = h + sum1 + choice + constants[i] + words[i];
            u32 sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            u32 majority = (a & b) ^ (a & c) ^ (b & c);
            u32 temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    std::array<u32, 8> m_state {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19,
    };
    std::array<u8, 64> m_buffer { };
    size_t m_buffer_size { 0 };
    u64 m_bit_count { 0 };
};

std::array<u8, 32> sha256(std::string_view input)
{
    Sha256 hash;
    hash.update(input);
    return hash.finish();
}

std::array<u8, 32> hmac_sha256(std::string_view key, std::string_view data)
{
    std::array<u8, 64> key_block { };
    if (key.size() > key_block.size()) {
        auto hashed_key = sha256(key);
        std::copy(hashed_key.begin(), hashed_key.end(), key_block.begin());
    } else if (!key.empty()) {
        std::memcpy(key_block.data(), key.data(), key.size());
    }

    std::array<char, 64> inner_pad { };
    std::array<char, 64> outer_pad { };
    for (size_t i = 0; i < key_block.size(); i++) {
        inner_pad[i] = static_cast<char>(key_block[i] ^ 0x36);
        outer_pad[i] = static_cast<char>(key_block[i] ^ 0x5c);
    }

    Sha256 inner;
    inner.update({ inner_pad.data(), inner_pad.size() });
    inner.update(data);
    auto inner_digest = inner.finish();

    Sha256 outer;
    outer.update({ outer_pad.data(), outer_pad.size() });
    outer.update({ reinterpret_cast<char const*>(inner_digest.data()), inner_digest.size() });
    return outer.finish();
}

} // anonymous namespace

Value VM::hash_new(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string() || string_bytes(argv[0]) != "sha256")
        return Value::nil();

    Value handle = dict(0, nullptr);
    dict_put(&handle, m_gc.make_string("__algorithm"), argv[0]);
    dict_put(&handle, m_gc.make_string("__data"), m_gc.make_string(""));
    return handle;
}

Value VM::hash_update(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_string())
        return Value::nil();
    Value key = m_gc.make_string("__data");
    Value existing = dict_get(&argv[0], key);
    if (!existing.is_string())
        return Value::nil();
    StringRef combined = existing.as_string()->str + argv[1].as_string()->str;
    dict_put(&argv[0], key, m_gc.make_string(combined));
    return Value::from_bool(true);
}

Value VM::hash_digest(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_bool())
        return Value::nil();
    Value algorithm = dict_get(&argv[0], m_gc.make_string("__algorithm"));
    Value data = dict_get(&argv[0], m_gc.make_string("__data"));
    if (!algorithm.is_string() || string_bytes(algorithm) != "sha256" || !data.is_string())
        return Value::nil();

    auto digest = sha256(string_bytes(data));
    if (argv[1].as_bool()) {
        std::string encoded = hex_string(digest.data(), digest.size());
        return m_gc.make_string(byte_string(encoded));
    }
    return m_gc.make_string(byte_string({ reinterpret_cast<char const*>(digest.data()), digest.size() }));
}

Value VM::hmac(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_string() || string_bytes(argv[0]) != "sha256")
        return Value::nil();
    auto digest = hmac_sha256(string_bytes(argv[1]), string_bytes(argv[2]));
    std::string encoded = hex_string(digest.data(), digest.size());
    return m_gc.make_string(byte_string(encoded));
}

} // fairuz::runtime
