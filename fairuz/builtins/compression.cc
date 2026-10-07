#include "../fvm.hpp"
#include "util.hpp"

#include <zlib.h>

namespace fairuz::runtime {

namespace {

bool gzip_compress(std::string_view input, int level, std::string& output)
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return false;

    z_stream stream { };
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    output.clear();
    std::array<char, 16384> buffer { };
    int status = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = deflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END) {
            deflateEnd(&stream);
            return false;
        }
        output.append(buffer.data(), buffer.size() - stream.avail_out);
    } while (status != Z_STREAM_END);

    return deflateEnd(&stream) == Z_OK;
}

bool gzip_decompress(std::string_view input, size_t maximum, std::string& output)
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return false;

    z_stream stream { };
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
        return false;

    output.clear();
    std::array<char, 16384> buffer { };
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        int status = inflate(&stream, Z_NO_FLUSH);
        size_t produced = buffer.size() - stream.avail_out;
        if (produced > maximum - std::min(maximum, output.size())) {
            inflateEnd(&stream);
            return false;
        }
        output.append(buffer.data(), produced);

        if (status == Z_STREAM_END)
            return inflateEnd(&stream) == Z_OK;
        if (status != Z_OK || (stream.avail_in == 0 && produced == 0)) {
            inflateEnd(&stream);
            return false;
        }
    }
}

} // anonymous namespace

Value VM::compress(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_int() || string_bytes(argv[0]) != "gzip")
        return Value::nil();
    i64 level = argv[2].as_int();
    if (level < 0 || level > 9)
        return Value::nil();
    std::string output;
    if (!gzip_compress(string_bytes(argv[1]), static_cast<int>(level), output))
        return Value::nil();
    return m_gc.make_string(byte_string(output));
}

Value VM::decompress(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_int() || string_bytes(argv[0]) != "gzip" || argv[2].as_int() <= 0)
        return Value::nil();
    std::string output;
    if (!gzip_decompress(string_bytes(argv[1]), static_cast<size_t>(argv[2].as_int()), output))
        return Value::nil();
    return m_gc.make_string(byte_string(output));
}

} // namespace fairuz::runtime
