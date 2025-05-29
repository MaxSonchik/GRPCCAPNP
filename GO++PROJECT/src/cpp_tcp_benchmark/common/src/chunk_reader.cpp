#include "chunk_reader.hpp"
#include <algorithm> // For std::min
#include <iostream>  // For error reporting

ChunkReader::ChunkReader(const std::string &filename, std::size_t chunk_size)
    : m_chunk_size(chunk_size), m_file_size(0), m_total_chunks(0),
      m_chunks_read_count(0), m_eof(false) {
  m_file_stream.open(filename, std::ios::binary | std::ios::in);
  if (!m_file_stream) {
    throw std::runtime_error("ChunkReader: Could not open file: " + filename);
  }

  m_file_stream.seekg(0, std::ios::end);
  m_file_size = static_cast<std::size_t>(m_file_stream.tellg());
  m_file_stream.seekg(0, std::ios::beg);

  if (m_file_size == 0) {
    m_total_chunks =
        0; // Or 1 if you consider an empty file as one "empty" chunk
    m_eof = true;
  } else {
    m_total_chunks =
        (m_file_size + m_chunk_size - 1) / m_chunk_size; // Ceiling division
  }
  // std::cout << "ChunkReader initialized for '" << filename << "'. File size:
  // " << m_file_size
  //           << " bytes. Chunk size: " << m_chunk_size << " bytes. Total
  //           chunks: " << m_total_chunks << std::endl;
}

ChunkReader::~ChunkReader() {
  if (m_file_stream.is_open()) {
    m_file_stream.close();
  }
}

std::vector<char> ChunkReader::read_next_chunk() {
  if (m_eof || !m_file_stream.is_open() || m_file_stream.eof()) {
    m_eof = true;
    return {};
  }

  std::vector<char> buffer(m_chunk_size);
  m_file_stream.read(buffer.data(), static_cast<std::streamsize>(m_chunk_size));
  std::streamsize bytes_read = m_file_stream.gcount();

  if (bytes_read == 0) { // No bytes read, could be EOF or error
    m_eof = true;        // Assume EOF if no bytes read
    return {};
  }

  if (bytes_read < static_cast<std::streamsize>(m_chunk_size)) {
    m_eof = true; // Reached end of file as less than chunk_size was read
  }

  buffer.resize(
      static_cast<std::size_t>(bytes_read)); // Resize to actual bytes read
  m_chunks_read_count++;
  return buffer;
}

bool ChunkReader::eof() const { return m_eof; }

std::size_t ChunkReader::total_chunks() const { return m_total_chunks; }

std::size_t ChunkReader::file_size() const { return m_file_size; }
std::size_t ChunkReader::chunks_read() const { return m_chunks_read_count; }
