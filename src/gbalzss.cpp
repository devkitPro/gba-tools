/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of gba-tools.
 *
 * gbalzss is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gbalzss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gbalzss.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file gbalzss.cpp
 *  @brief GBA LZSS Encoder/Decoder
 */
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <getopt.h>
#include <libgen.h>
#include <cstddef>

namespace
{

/** @brief LZSS maximum encodable size */
#define LZSS_MAX_ENCODE_LEN 0x00FFFFFF

/** @brief LZSS maximum (theoretical) decodable size
 *  (LZSS_MAX_ENCODE_LEN+1)*9/8 + 4 - 1
 */
#define LZSS_MAX_DECODE_LEN 0x01B00003

/** @brief LZ10 maximum match length */
#define LZ10_MAX_LEN  18

/** @brief LZ10 maximum displacement */
#define LZ10_MAX_DISP 4096

/** @brief LZ11 maximum match length */
#define LZ11_MAX_LEN  65808

/** @brief LZ11 maximum displacement */
#define LZ11_MAX_DISP 4096

/** @brief LZ compression mode */
enum LZSS_t
{
  LZ10 = 0x10, ///< LZ10 compression
  LZ11 = 0x11, ///< LZ11 compression
};

/** @brief Buffer object */
typedef std::vector<uint8_t> Buffer;

/** @brief Find last instance of a byte in a buffer
 *  @param[in] first Beginning of buffer
 *  @param[in] last  End of buffer
 *  @param[in] val   Byte to find
 *
 *  @returns iterator to found byte
 *  @retval last if no match found
 */
Buffer::const_iterator
rfind(Buffer::const_iterator first, Buffer::const_iterator last,
      const uint8_t &val)
{
  assert(last >= first);

  auto it = last;
  while(--it >= first)
  {
    if(*it == val)
      return it;
  }

  return last;
}

/** @brief Find best buffer match
 *  @param[in]  source   Source buffer
 *  @param[in]  it       Position in source buffer
 *  @param[in]  len      Maximum length to match
 *  @param[in]  max_disp Maximum displacement
 *  @param[in]  vram     VRAM-safe
 *  @param[out] outlen   Length of match
 *  @returns Iterator to best match
 *  @retval source.cend() for no match
 */
Buffer::const_iterator
find_best_match(const Buffer &source, Buffer::const_iterator it, size_t len,
                size_t max_disp, bool vram, size_t &outlen)
{
  auto begin = source.cbegin();
  auto end   = source.cend();

  assert(it > source.cbegin());
  assert(it < source.cend());

  // clamp start to maximum displacement from buffer
  if(it - begin > static_cast<ptrdiff_t>(max_disp))
    begin = it - max_disp;

  // clamp len to end of buffer
  if(end - it < static_cast<ptrdiff_t>(len))
    len = end - it;

  auto   best_start = it;
  size_t best_len = 0;

  // find nearest matching start byte
  auto last_p = it;
  auto p = rfind(begin, last_p, *it);
  while(p != last_p)
  {
    // find length of match
    size_t test_len = 1;
    for(size_t i = 1; i < len; ++i)
    {
      if(*(p+i) == *(it+i))
        ++test_len;
      else
        break;
    }

    // vram requires displacement != 1
    if(vram && (it - p) == 1)
      test_len = 0;

    if(test_len >= best_len)
    {
      // this match is the best so far, so save it
      best_start = p;
      best_len   = test_len;
    }

    // if we maximized the match, stop here
    if(best_len == len)
      break;

    // find next nearest matching byte and try again
    last_p = p;
    p = rfind(begin, last_p, *it);
  }

  if(best_len)
  {
    // we found a match, so return it
    outlen = best_len;
    return best_start;
  }

  // no match found
  outlen = 0;
  return source.cend();
}

/** @brief Output a GBA-style compression header
 *  @param[out] header Output header
 *  @param[in]  type   Compression type
 *  @param[in]  size   Uncompressed data size
 */
void
header(Buffer &buffer, uint8_t type, size_t size)
{
  buffer.push_back(type);
  buffer.push_back(size >>  0);
  buffer.push_back(size >>  8);
  buffer.push_back(size >> 16);
}

/** @brief LZ10/LZ11 compression
 *  @param[in] source Source buffer
 *  @param[in] mode   LZ mode
 *  @param[in] vram   VRAM-safe
 *  @returns Compressed buffer
 */
Buffer
lzss_encode(const Buffer &source, LZSS_t mode, bool vram)
{
  // get maximum match length
  const size_t max_len  = mode == LZ10 ? LZ10_MAX_LEN  : LZ11_MAX_LEN;

  // get maximum displacement
  const size_t max_disp = mode == LZ10 ? LZ10_MAX_DISP : LZ11_MAX_DISP;

  assert(mode == LZ10 || mode == LZ11);

  // create output buffer
  Buffer result;

  // append compression header
  header(result, mode, source.size());

  // reserve an encode byte in output buffer
  size_t code_pos = result.size();
  result.push_back(0);

  // initialize shift
  size_t shift = 8;

  // encode every byte
  auto it = source.cbegin();
  auto end = source.cend();
  while(it < end)
  {
    if(shift == 0)
    {
      // we need to encode more data, so add a new code byte
      shift = 8;
      code_pos = result.size();
      result.push_back(0);
    }

    // advance code byte bit position
    if(shift != 0)
      --shift;

    const size_t len = end - it;
    auto         tmp = source.cend();
    size_t       tmplen = 0;

    if(it == source.cbegin())
    {
      // beginning of stream must be primed with at least one value
      tmplen = 1;
    }
    else
    {
      // find best match
      tmp = find_best_match(source, it, std::min(len, max_len), max_disp, vram,
                            tmplen);
      if(tmp != source.cend())
      {
        assert(!vram || tmp - it != 1);
        assert(tmp >= source.cbegin());
        assert(tmp < it);
        assert(it - tmp <= static_cast<ptrdiff_t>(max_disp));
        assert(tmplen <= max_len);
        assert(tmplen <= len);
        assert(std::equal(it, it+tmplen, tmp));
      }
    }

    if(tmplen > 2 && tmplen < len)
    {
      // this match is long enough to be compressed; let's check if it's
      // cheaper to encode this byte as a copy and start compression at the
      // next byte
      size_t skip_len, next_len;

      // get best match starting at the next byte
      find_best_match(source, it+1, std::min(len-1, max_len), max_disp, vram,
                      skip_len);

      // check if the match is too small to compress
      if(skip_len < 3)
        skip_len = 1;

      // get best match for data following the current compressed chunk
      find_best_match(source, it+tmplen, std::min(len-tmplen, max_len),
                      max_disp, vram, next_len);

      // check if the match is too small to compress
      if(next_len < 3)
        next_len = 1;

      // if compressing this chunk and the next chunk is less valuable than
      // skipping this byte and starting compression at the next byte, mark
      // this byte as being needed to copy
      if(tmplen + next_len <= skip_len + 1)
        tmplen = 1;
    }

    if(tmplen < 3)
    {
      // this is a copy chunk; append this byte to the output buffer
      result.push_back(*it);

      // only one byte is copied
      tmplen = 1;
    }
    else if(mode == LZ10)
    {
      // mark this chunk as compressed
      assert(code_pos < result.size());
      result[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = it - tmp - 1;
      assert(tmplen-3 <= 0xF);
      assert(disp <= 0xFFF);
      result.push_back(((tmplen-3) << 4) | (disp >> 8));
      result.push_back(disp);
    }
    else if(tmplen <= 0x10)
    {
      // mark this chunk as compressed
      assert(code_pos < result.size());
      result[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = it - tmp - 1;
      assert(tmplen > 2);
      assert(tmplen-1 <= 0xF);
      assert(disp <= 0xFFF);
      result.push_back(((tmplen-1) << 4) | (disp >> 8));
      result.push_back(disp);
    }
    else if(tmplen <= 0x110)
    {
      // mark this chunk as compressed
      assert(code_pos < result.size());
      result[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = it - tmp - 1;
      assert(tmplen >= 0x11);
      assert(tmplen-0x11 <= 0xFF);
      assert(disp <= 0xFFF);
      result.push_back((tmplen-0x11) >> 4);
      result.push_back(((tmplen-0x11) << 4) | (disp >> 8));
      result.push_back(disp);
    }
    else
    {
      // mark this chunk as compressed
      assert(code_pos < result.size());
      result[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = it - tmp - 1;
      assert(tmplen >= 0x111);
      assert(tmplen-0x111 <= 0xFFFF);
      assert(disp <= 0xFFF);
      result.push_back((1 << 4) | (tmplen-0x111) >> 12);
      result.push_back(((tmplen-0x111) >> 4));
      result.push_back(((tmplen-0x111) << 4) | (disp >> 8));
      result.push_back(disp);
    }

    // advance input buffer
    it += tmplen;
  }

  // pad the output buffer to 4 bytes
  if(result.size() & 0x3)
    result.resize((result.size()+3) & ~0x3);

  // return the output data
  return result;
}

/** @brief LZ10 compression
 *  @param[in] source Source buffer
 *  @param[in] vram   VRAM-safe
 *  @returns Compressed buffer
 */
Buffer
lz10_encode(const Buffer &source, bool vram)
{
  return lzss_encode(source, LZ10, vram);
}

/** @brief LZ11 compression
 *  @param[in] source Source buffer
 *  @param[in] vram   VRAM-safe
 *  @returns Compressed buffer
 */
Buffer
lz11_encode(const Buffer &source, bool vram)
{
  return lzss_encode(source, LZ11, vram);
}

/** @brief LZ10 Decompression
 *  @param[in] source Source buffer
 *  @param[in] vram   VRAM-safe
 *  @returns Decompressed buffer
 */
Buffer
lz10_decode(const Buffer &source, bool vram)
{
  if(source.size() < 4 || source[0] != LZ10)
    throw std::runtime_error("Error: Invalid LZ10 header");

  size_t size = source[1] | (source[2] << 8) | (source[3] << 16);

  bool printed_error = false;
  bool printed_vram_error = false;

  auto    src   = source.cbegin() + 4;
  uint8_t flags = 0;
  uint8_t mask  = 0;

  Buffer result;

  while(size > 0)
  {
    if(mask == 0)
    {
      // read in the flags data
      // from bit 7 to bit 0:
      //     0: raw byte
      //     1: compressed block
      flags = *src++;
      mask  = 0x80;
    }

    if(flags & mask) // compressed block
    {
      size_t len  = (((*src) & 0xF0) >> 4) + 3;
      size_t disp = ((*src++) & 0x0F) << 8;
      disp |= *src++;
      ++disp;

      if(len > size)
      {
        if(!printed_error)
        {
          std::fprintf(stderr, "Warning: Badly encoded LZ10 stream; compressed "
                       "block exceeds output length specified by header. "
                       "Truncating output.\n");
          printed_error = true;
        }

        // truncate output
        len = size;
      }

      if(result.size() < disp)
        throw std::runtime_error("Error: Badly encoded LZ10 stream; encoded "
                                 "displacement causes read prior to start of "
                                 "output buffer.");

      if(vram && !printed_vram_error)
      {
        if(disp == 1)
        {
          std::fprintf(stderr, "Warning: LZ10 stream is not vram safe.\n");
          printed_vram_error = true;
        }
      }
      
      size -= len;

      // for len, copy data from the displacement
      // to the current buffer position
      while(len-- > 0)
        result.push_back(*(std::end(result)-disp));
    }
    else // uncompressed block
    {
      // copy a raw byte from the input to the output
      result.push_back(*src++);
      --size;
    }

    mask >>= 1;
  }

  return result;
}

/** @brief LZ11 Decompression
 *  @param[in] source Source buffer
 *  @param[in] vram   VRAM-safe
 *  @returns Decompressed buffer
 */
Buffer
lz11_decode(const Buffer &source, bool vram)
{
  if(source.size() < 4 || source[0] != LZ11)
    throw std::runtime_error("Error: Invalid LZ11 header");

  size_t size = source[1] | (source[2] << 8) | (source[3] << 16);

  bool printed_error = false;
  bool printed_vram_error = false;

  auto    src   = source.cbegin() + 4;
  uint8_t flags = 0;
  uint8_t mask  = 0;

  Buffer result;

  while(size > 0)
  {
    if(mask == 0)
    {
      // read in the flags data
      // from bit 7 to bit 0:
      //     0: raw byte
      //     1: compressed block
      flags = *src++;
      mask  = 0x80;
    }

    if(flags & mask) // compressed block
    {
      size_t len;
      switch((*src) >> 4)
      {
        case 0: // extended block
          len   = (*src++) << 4;
          len  |= ((*src) >> 4);
          len  += 0x11;
          break;

        case 1: // extra extended block
          len   = ((*src++) & 0x0F) << 12;
          len  |= (*src++) << 4;
          len  |= ((*src) >> 4);
          len  += 0x111;
          break;

        default: // normal block
          len   = ((*src) >> 4) + 1;
          break;
      }

      size_t disp = ((*src++) & 0x0F) << 8;
      disp |= *src++;
      ++disp;

      if(len > size)
      {
        if(!printed_error)
        {
          std::fprintf(stderr, "Warning: Badly encoded LZ11 stream; compressed "
                       "block exceeds output length specified by header. "
                       "Truncating output.\n");
          printed_error = true;
        }

        // truncate output
        len = size;
      }

      if(result.size() < disp)
        throw std::runtime_error("Error: Badly encoded LZ11 stream; encoded "
                                 "displacement causes read prior to start of "
                                 "output buffer.");

      if(vram && !printed_vram_error)
      {
        if(disp == 1)
        {
          std::fprintf(stderr, "Warning: LZ10 stream is not vram safe.\n");
          printed_vram_error = true;
        }
      }
 
      size -= len;

      // for len, copy data from the displacement
      // to the current buffer position
      while(len-- > 0)
        result.push_back(*(std::end(result)-disp));
    }
    else // uncompressed block
    {
      // copy a raw byte from the input to the output
      result.push_back(*src++);
      --size;
    }

    mask >>= 1;
  }

  return result;
}

/** @brief Read input file
 *  @param[in] fp    Input file stream
 *  @param[in] limit Maximum file size to read
 *  @returns Buffer containing file contents
 */
Buffer read_file(FILE *fp, size_t limit)
{
  Buffer buffer;
  Buffer tmp(4096);

  ssize_t rc;
  do
  {
    // read data into tmp buffer
    rc = std::fread(tmp.data(), 1, tmp.size(), fp);
    if(rc > 0)
    {
      // append to result buffer
      buffer.insert(std::end(buffer), std::begin(tmp), std::begin(tmp)+rc);

      if(buffer.size() > limit)
        throw std::runtime_error("Error: Input file too large.\n");
    }
  } while(rc > 0);

  if(rc < 0)
    throw std::runtime_error("Error: Failed to read file");

  return buffer;
}

/** @brief Write output file
 *  @param[in] fp    Output file stream
 *  @param[in] limit Maximum file size to write
 *  @returns Whether successfully written
 */
bool write_file(FILE *fp, const Buffer &buffer)
{
  auto it = std::begin(buffer);
  while(it < std::end(buffer))
  {
    // write to file
    ssize_t rc = std::fwrite(&*it, 1, std::end(buffer) - it, fp);
    if(rc <= 0)
      return false;

    it += rc;
  }

  return true;
}

/** @brief Print program usage
 *  @param[in] fp      File stream to write usage
 *  @param[in] program Program name
 */
void usage(FILE *fp, const char *program)
{
  std::fprintf(fp,
    "Usage: %s [-h|--help] [--lz11] [--vram] <d|e> <infile> <outfile>\n"
    "\tOptions:\n"
    "\t\t-h, --help\tShow this help\n"
    "\t\t--lz11    \tCompress using LZ11 instead of LZ10\n"
    "\t\t--vram    \tGenerate VRAM-safe output (required by GBA BIOS)\n"
    "\n"
    "\tArguments\n"
    "\t\te         \tCompress <infile> into <outfile>\n"
    "\t\td         \tDecompress <infile> into <outfile>\n"
    "\t\t<infile>  \tInput file (use - for stdin)\n"
    "\t\t<outfile> \tOutput file (use - for stdout)\n",
    program);
}

/** @brief Program long options */
const struct option long_options[] =
{
  { "help",    no_argument, nullptr, 'h', },
  { "lz11",    no_argument, nullptr, '1', },
  { "vram",    no_argument, nullptr, 'v', },
  { nullptr,   no_argument, nullptr,   0, },
};

}

int main(int argc, char *argv[])
{
  // get program name
  const char *program = ::basename(argv[0]);

  bool lz11 = false;
  bool vram = false;

  // parse options
  int c;
  while((c = ::getopt_long(argc, argv, "h", long_options, nullptr)) != -1)
  {
    switch(c)
    {
      case 'h':
        usage(stdout, program);
        return EXIT_SUCCESS;

      case '1':
        lz11 = true;
        break;

      case 'v':
        vram = true;
        break;

      default:
        std::fprintf(stderr, "Error: Invalid option '%c'\n", optopt);
        usage(stderr, program);
        return EXIT_FAILURE;
    }
  }

  // check for valid encode/decode non-option
  if(argc - optind != 3
  || std::strlen(argv[optind]) > 1
  || (std::tolower(*argv[optind]) != 'e' && std::tolower(*argv[optind]) != 'd'))
  {
    usage(stderr, program);
    return EXIT_FAILURE;
  }

  // get program non-options
  bool encode = std::tolower(*argv[optind++]) == 'e';
  const char *infile = argv[optind++];
  const char *outfile = argv[optind++];

  // open input file
  FILE *fp;
  if(std::strlen(infile) == 1 && *infile == '-')
    fp = stdin;
  else
    fp = std::fopen(infile, "rb");
  if(!fp)
  {
    std::fprintf(stderr, "Error: Failed to open '%s' for reading\n", infile);
    return EXIT_FAILURE;
  }

  Buffer buffer;

  // read input file
  try
  {
    buffer = read_file(fp, encode ? LZSS_MAX_ENCODE_LEN : LZSS_MAX_DECODE_LEN);
  }
  catch(const std::runtime_error &e)
  {
    std::fprintf(stderr, "%s: %s\n", infile, e.what());
    std::fclose(fp);
    return EXIT_FAILURE;
  }
  catch(...)
  {
    std::fprintf(stderr, "%s: Error: unhandled exception\n", infile);
    std::fclose(fp);
    return EXIT_FAILURE;
  }

  // close input file
  std::fclose(fp);

  // process input file
  try
  {
    if(encode)
      buffer = (lz11 ? lz11_encode : lz10_encode)(buffer, vram);
    else
      buffer = (lz11 ? lz11_decode : lz10_decode)(buffer, vram);
  }
  catch(const std::runtime_error &e)
  {
    std::fprintf(stderr, "%s: %s\n", infile, e.what());
    return EXIT_FAILURE;
  }
  catch(...)
  {
    std::fprintf(stderr, "%s: Error: unhandled exception\n", infile);
    return EXIT_FAILURE;
  }

  // open output file
  if(std::strlen(outfile) == 1 && *outfile == '-')
    fp = stdout;
  else
    fp = std::fopen(outfile, "wb");
  if(!fp)
  {
    std::fprintf(stderr, "Error: Failed to open '%s' for writing\n", outfile);
    return EXIT_FAILURE;
  }

  // write output file
  if(!write_file(fp, buffer))
  {
    std::fprintf(stderr, "Error: Failed to write '%s'\n", outfile);
    std::fclose(fp);
    return EXIT_FAILURE;
  }

  // close output file
  std::fclose(fp);

  return EXIT_SUCCESS;
}
