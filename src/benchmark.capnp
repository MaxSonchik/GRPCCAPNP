# benchmark.capnp
@0xf210e7061592269a;

struct Chunk {
  data @0 :Data;
}

interface FileProcessor {
  startStreaming @0 () -> (handler :ChunkHandler);

  interface ChunkHandler {
    processChunk @0 (request :Chunk) -> (response :Chunk);
    doneStreaming @1 () -> ();
  }
}
