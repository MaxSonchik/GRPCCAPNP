// Package metrics предоставляет структуру и методы для сбора и сохранения метрик бенчмарка.
package metrics

import (
	"src/go_tcp_benchmark/pkg/config"
	"encoding/csv"
	"fmt"
	"log"
	"os"
	"runtime" // Для сбора информации о памяти
	"strconv"
	"sync" // Для мьютекса, если потребуется потокобезопасный доступ
	"time"
)

// ChunkRTTMetric хранит RTT для одного чанка.
type ChunkRTTMetric struct {
	ChunkIndex int
	RTT        time.Duration // time.Duration - это тип для представления длительности времени
}

// MetricsAggregator собирает все метрики.
type MetricsAggregator struct {
	language            string
	totalFileSize       int64
	chunkSize           int
	startTime           time.Time // time.Time - представляет момент времени
	endTime             time.Time
	totalBytesProcessed int64
	chunksSent          int // Для клиента - сколько чанков успешно отправлено и проверено
	chunksVerifiedOK    int
	chunksVerifiedFail  int
	chunkRTTs           []ChunkRTTMetric
	clientPeakMemoryMB  uint64 // Пиковое потребление памяти (приблизительно)
	// CPU usage is harder to get programmatically for the current Go process without CGo or external libs.
	// We'll recommend external measurement for CPU for Go client, similar to server.
	// clientTotalCPUTimeSeconds float64

	mutex sync.Mutex // Для безопасного доступа из нескольких горутин, если потребуется
}

// NewMetricsAggregator создает новый экземпляр MetricsAggregator.
func NewMetricsAggregator(lang string, totalFileSize int64, chunkSize int) *MetricsAggregator {
	return &MetricsAggregator{
		language:      lang,
		totalFileSize: totalFileSize,
		chunkSize:     chunkSize,
		chunkRTTs:     make([]ChunkRTTMetric, 0, totalFileSize/int64(chunkSize)+1),
	}
}

// StartTimer запускает общий таймер бенчмарка.
func (m *MetricsAggregator) StartTimer() {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.startTime = time.Now()
}

// StopTimer останавливает общий таймер и собирает метрики памяти.
func (m *MetricsAggregator) StopTimer() {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	if m.endTime.IsZero() { // Убедимся, что останавливаем таймер только один раз
		m.endTime = time.Now()
		m.collectMemoryStats()
	}
}

// RecordChunkProcessed записывает количество обработанных байт (отправленных клиентом).
func (m *MetricsAggregator) RecordChunkProcessed(size int) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.totalBytesProcessed += int64(size)
}

// RecordChunkVerified записывает результат верификации чанка.
func (m *MetricsAggregator) RecordChunkVerified(success bool) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.chunksSent++ // Считаем, что чанк был "обработан" клиентом (отправлен и получен ответ)
	if success {
		m.chunksVerifiedOK++
	} else {
		m.chunksVerifiedFail++
	}
}

// RecordChunkRTT записывает RTT для чанка.
func (m *MetricsAggregator) RecordChunkRTT(chunkIndex int, rtt time.Duration) {
	m.mutex.Lock()
	defer m.mutex.Unlock()
	m.chunkRTTs = append(m.chunkRTTs, ChunkRTTMetric{ChunkIndex: chunkIndex, RTT: rtt})
}

// collectMemoryStats собирает статистику по памяти.
// runtime.ReadMemStats читает статистику выделения памяти из среды выполнения Go.
func (m *MetricsAggregator) collectMemoryStats() {
	// Эта функция должна вызываться под мьютексом, что обеспечивается в StopTimer
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	// stats.Sys - это общее количество байт памяти, полученных от ОС.
	// Это может служить грубой оценкой пикового потребления.
	m.clientPeakMemoryMB = stats.Sys / (1024 * 1024)
}

// PrintSummary выводит итоговую статистику в консоль.
func (m *MetricsAggregator) PrintSummary() {
	m.mutex.Lock()
	defer m.mutex.Unlock()

	duration := m.endTime.Sub(m.startTime) // m.endTime - m.startTime
	throughputMBps := float64(0)
	if duration.Seconds() > 0 {
		// Пропускная способность считается по totalBytesProcessed,
		// что равно размеру исходного файла, если все чанки переданы.
		throughputMBps = (float64(m.totalBytesProcessed) / (1024 * 1024)) / duration.Seconds()
	}

	log.Println("----------------------------------------------------")
	log.Printf("%s Benchmark Summary:\n", m.language)
	log.Printf("Total time: %.3f seconds\n", duration.Seconds())
	log.Printf("Total data processed: %d bytes (%.2f MB)\n", m.totalBytesProcessed, float64(m.totalBytesProcessed)/(1024*1024))
	log.Printf("Throughput: %.3f MB/s\n", throughputMBps)
	log.Printf("Chunks sent/processed: %d\n", m.chunksSent)
	log.Printf("Chunks verified OK: %d\n", m.chunksVerifiedOK)
	log.Printf("Chunks verified FAILED: %d\n", m.chunksVerifiedFail)
	log.Printf("Client Peak Memory (Sys): %d MB\n", m.clientPeakMemoryMB)
	log.Println("Note: CPU usage should be measured externally for Go client.")
	log.Println("----------------------------------------------------")
}

// SaveToCSV сохраняет метрики в CSV файлы.
func (m *MetricsAggregator) SaveToCSV() error {
	m.mutex.Lock()
	defer m.mutex.Unlock()

	// Сохранение общих метрик
	overallFile, err := os.Create(config.CSVOverallMetricsFile)
	if err != nil {
		return fmt.Errorf("failed to create overall metrics CSV '%s': %w", config.CSVOverallMetricsFile, err)
	}
	defer overallFile.Close()

	overallWriter := csv.NewWriter(overallFile)
	defer overallWriter.Flush()

	duration := m.endTime.Sub(m.startTime)
	throughputMBps := float64(0)
	if duration.Seconds() > 0 {
		throughputMBps = (float64(m.totalBytesProcessed) / (1024 * 1024)) / duration.Seconds()
	}

	headersOverall := []string{"language", "total_time_seconds", "total_data_processed_bytes", "throughput_mbps", "chunks_sent", "chunks_verified_ok", "chunks_verified_fail", "client_peak_memory_mb_sys"}
	if err := overallWriter.Write(headersOverall); err != nil {
		return fmt.Errorf("failed to write overall headers to CSV: %w", err)
	}
	recordOverall := []string{
		m.language,
		fmt.Sprintf("%.3f", duration.Seconds()),
		strconv.FormatInt(m.totalBytesProcessed, 10),
		fmt.Sprintf("%.3f", throughputMBps),
		strconv.Itoa(m.chunksSent),
		strconv.Itoa(m.chunksVerifiedOK),
		strconv.Itoa(m.chunksVerifiedFail),
		strconv.FormatUint(m.clientPeakMemoryMB, 10),
	}
	if err := overallWriter.Write(recordOverall); err != nil {
		return fmt.Errorf("failed to write overall record to CSV: %w", err)
	}
	log.Printf("Overall metrics saved to %s\n", config.CSVOverallMetricsFile)

	// Сохранение RTT по чанкам
	rttFile, err := os.Create(config.CSVChunkRTTMetricsFile)
	if err != nil {
		return fmt.Errorf("failed to create RTT metrics CSV '%s': %w", config.CSVChunkRTTMetricsFile, err)
	}
	defer rttFile.Close()

	rttWriter := csv.NewWriter(rttFile)
	defer rttWriter.Flush()

	headersRTT := []string{"language", "chunk_index", "rtt_milliseconds"}
	if err := rttWriter.Write(headersRTT); err != nil {
		return fmt.Errorf("failed to write RTT headers to CSV: %w", err)
	}
	for _, rttMetric := range m.chunkRTTs {
		recordRTT := []string{
			m.language,
			strconv.Itoa(rttMetric.ChunkIndex),
			fmt.Sprintf("%.3f", float64(rttMetric.RTT.Microseconds())/1000.0), // RTT в миллисекундах
		}
		if err := rttWriter.Write(recordRTT); err != nil {
			return fmt.Errorf("failed to write RTT record to CSV: %w", err)
		}
	}
	log.Printf("Chunk RTT metrics saved to %s\n", config.CSVChunkRTTMetricsFile)
	return nil
}
