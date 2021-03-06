#include <IO/CompressedReadBuffer.h>
#include <IO/CompressedStream.h>
#include <IO/LZ4_decompress_faster.h>


namespace DB
{

bool CompressedReadBuffer::nextImpl()
{
    size_t size_decompressed;
    size_t size_compressed_without_checksum;
    size_compressed = readCompressedData(size_decompressed, size_compressed_without_checksum);
    if (!size_compressed)
        return false;

    memory.resize(size_decompressed + LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER);
    working_buffer = Buffer(memory.data(), &memory[size_decompressed]);

    decompress(working_buffer.begin(), size_decompressed, size_compressed_without_checksum);

    return true;
}

size_t CompressedReadBuffer::readBig(char * to, size_t n)
{
    size_t bytes_read = 0;

    /// If there are unread bytes in the buffer, then we copy necessary to `to`.
    if (pos < working_buffer.end())
        bytes_read += read(to, std::min(static_cast<size_t>(working_buffer.end() - pos), n));

    /// If you need to read more - we will, if possible, uncompress at once to `to`.
    while (bytes_read < n)
    {
        size_t size_decompressed;
        size_t size_compressed_without_checksum;

        if (!readCompressedData(size_decompressed, size_compressed_without_checksum))
            return bytes_read;

        /// If the decompressed block is placed entirely where it needs to be copied.
        if (size_decompressed + LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER <= n - bytes_read)
        {
            decompress(to + bytes_read, size_decompressed, size_compressed_without_checksum);
            bytes_read += size_decompressed;
            bytes += size_decompressed;
        }
        else
        {
            bytes += offset();
            memory.resize(size_decompressed + LZ4::ADDITIONAL_BYTES_AT_END_OF_BUFFER);
            working_buffer = Buffer(memory.data(), &memory[size_decompressed]);
            pos = working_buffer.begin();

            decompress(working_buffer.begin(), size_decompressed, size_compressed_without_checksum);

            bytes_read += read(to + bytes_read, n - bytes_read);
            break;
        }
    }

    return bytes_read;
}

}
