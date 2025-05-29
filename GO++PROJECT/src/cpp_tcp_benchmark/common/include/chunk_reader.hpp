#ifndef CHUNK_READER_HPP
#define CHUNK_READER_HPP

#include <cstddef> // For size_t
#include <fstream>
#include <string>
#include <vector>

class ChunkReader {
public:
  ChunkReader(const std::string &filename, std::size_t chunk_size);
  ~ChunkReader();

  std::vector<char> read_next_chunk();
  bool eof() const;
  std::size_t total_chunks() const;
  std::size_t file_size() const;
  std::size_t chunks_read() const;

private:
  mutable std::ifstream m_file_stream;
  std::size_t m_chunk_size;
  std::size_t m_file_size;
  std::size_t m_total_chunks;
  std::size_t m_chunks_read_count;
  bool m_eof;
};

#endif // CHUNK_READER_HPP
