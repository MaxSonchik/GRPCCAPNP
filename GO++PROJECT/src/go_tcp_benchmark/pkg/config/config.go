// Package config содержит константы конфигурации для бенчмарка.
package config

const (
	TotalFileSize          int64  = 10 * 1024 * 1024 * 1024 // 10 GB (используем int64 для размеров файлов)
	ChunkSize              int    = 64 * 1024               // 64 KB (используем int для размеров чанков в памяти)
	TCPServerPort          string = "12346"                 // Порт для Go TCP сервера (отличается от C++ для параллельного запуска)
	DefaultServerIP        string = "127.0.0.1"
	TestFileName           string = "test_file_10gb.dat" // Имя файла для генерации и тестов
	CSVOverallMetricsFile  string = "go_overall_metrics.csv"
	CSVChunkRTTMetricsFile string = "go_chunk_rtt_metrics.csv"
)
