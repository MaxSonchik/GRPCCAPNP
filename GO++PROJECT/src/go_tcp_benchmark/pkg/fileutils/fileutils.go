// Package fileutils предоставляет утилиты для работы с файлами,
// включая генерацию тестового файла и чтение его по чанкам.
package fileutils

import (
	//"src/go_tcp_benchmark/pkg/config" // Импорт нашего пакета config
	"crypto/rand"                                   // Для генерации случайных байт
	"io"
	"log"
	"os" // Для работы с файловой системой
)

// GenerateTestFileIfNotExists создает тестовый файл заданного размера,
// если он не существует или имеет некорректный размер.
func GenerateTestFileIfNotExists(filename string, targetSize int64) {
	// os.Stat возвращает информацию о файле (FileInfo) и ошибку.
	info, err := os.Stat(filename)
	if err == nil && info.Size() == targetSize {
		log.Printf("Test file '%s' already exists with correct size %d bytes.\n", filename, targetSize)
		return
	}

	if err == nil && info.Size() != targetSize {
		log.Printf("Test file '%s' exists but has incorrect size (%d vs %d). Regenerating.\n", filename, info.Size(), targetSize)
	} else if os.IsNotExist(err) {
		log.Printf("Test file '%s' does not exist. Generating with size %d bytes.\n", filename, targetSize)
	} else if err != nil {
		log.Fatalf("Error checking test file '%s': %v. Aborting.\n", filename, err)
	}

	// os.Create создает или перезаписывает файл. Возвращает *os.File и ошибку.
	file, err := os.Create(filename)
	if err != nil {
		log.Fatalf("Failed to create test file '%s': %v\n", filename, err)
	}
	// defer file.Close() гарантирует, что файл будет закрыт при выходе из функции,
	// даже если произойдет паника или return.
	defer file.Close()

	// Генерируем случайные данные чанками и записываем в файл.
	// Это эффективнее, чем держать весь 10 ГБ файл в памяти.
	buffer := make([]byte, 1024*1024) // Буфер 1 МБ для генерации
	written := int64(0)
	for written < targetSize {
		bytesToRead := int64(len(buffer))
		if written+bytesToRead > targetSize {
			bytesToRead = targetSize - written
		}
		// rand.Read заполняет срез случайными байтами.
		_, err := rand.Read(buffer[:bytesToRead])
		if err != nil {
			log.Fatalf("Failed to generate random data: %v\n", err)
		}
		// file.Write записывает данные из буфера в файл.
		n, err := file.Write(buffer[:bytesToRead])
		if err != nil {
			log.Fatalf("Failed to write to test file: %v\n", err)
		}
		written += int64(n)
	}
	log.Printf("Successfully generated test file '%s' with size %d bytes.\n", filename, targetSize)
}

// ChunkReader читает файл по частям (чанкам).
type ChunkReader struct {
	file        *os.File
	chunkSize   int
	fileSize    int64
	bytesRead   int64
	totalChunks int64
	eofReached  bool
}

// NewChunkReader создает новый ChunkReader.
func NewChunkReader(filename string, chunkSize int) (*ChunkReader, error) {
	file, err := os.Open(filename) // Открываем файл только для чтения.
	if err != nil {
		return nil, err
	}

	info, err := file.Stat()
	if err != nil {
		file.Close() // Не забываем закрыть файл при ошибке.
		return nil, err
	}
	fileSize := info.Size()

	totalChunks := fileSize / int64(chunkSize)
	if fileSize%int64(chunkSize) != 0 {
		totalChunks++
	}
	if fileSize == 0 { // Обработка пустого файла
		totalChunks = 0
	}

	return &ChunkReader{
		file:        file,
		chunkSize:   chunkSize,
		fileSize:    fileSize,
		totalChunks: totalChunks,
	}, nil
}

// ReadNextChunk читает следующий чанк из файла.
// Возвращает срез байт и ошибку. io.EOF сигнализирует о конце файла.
func (cr *ChunkReader) ReadNextChunk() ([]byte, error) {
	if cr.eofReached {
		return nil, io.EOF
	}

	buffer := make([]byte, cr.chunkSize)
	// file.Read читает до len(buffer) байт в buffer.
	// Возвращает количество прочитанных байт и ошибку.
	// io.EOF возвращается, если прочитано 0 байт и достигнут конец файла.
	n, err := cr.file.Read(buffer)
	if err != nil {
		if err == io.EOF {
			cr.eofReached = true
			// Если что-то было прочитано перед EOF (n > 0), возвращаем эти данные.
			// Если n == 0, это чистый EOF, и мы вернем его без данных.
			if n > 0 {
				cr.bytesRead += int64(n)
				return buffer[:n], nil
			}
		}
		return nil, err // Возвращаем ошибку (включая io.EOF, если n==0).
	}

	cr.bytesRead += int64(n)
	// Если прочитали меньше, чем размер чанка, но не EOF, это странно,
	// но мы все равно возвращаем то, что прочитали. Следующий вызов, вероятно, вернет EOF.
	// Если n < cr.chunkSize, это может быть последний чанк.
	if cr.bytesRead >= cr.fileSize {
		cr.eofReached = true
	}
	return buffer[:n], nil
}

// TotalChunks возвращает общее количество чанков, на которое разбит файл.
func (cr *ChunkReader) TotalChunks() int64 {
	return cr.totalChunks
}

// FileSize возвращает общий размер файла.
func (cr *ChunkReader) FileSize() int64 {
	return cr.fileSize
}

// EOF возвращает true, если конец файла был достигнут.
func (cr *ChunkReader) EOF() bool {
	return cr.eofReached
}

// Close закрывает файл. Должен вызываться, когда ChunkReader больше не нужен.
func (cr *ChunkReader) Close() error {
	if cr.file != nil {
		return cr.file.Close()
	}
	return nil
}
