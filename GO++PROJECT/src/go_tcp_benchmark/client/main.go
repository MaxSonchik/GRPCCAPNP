package main

import (
	"src/go_tcp_benchmark/pkg/config"
	"src/go_tcp_benchmark/pkg/fileutils"
	"src/go_tcp_benchmark/pkg/messaging"
	"src/go_tcp_benchmark/pkg/metrics"
	"src/go_tcp_benchmark/pkg/reversal"
	"bytes" // Для bytes.Equal
	"flag"  // Для парсинга аргументов командной строки
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"time"
)

func main() {

	serverIP := flag.String("server_ip", config.DefaultServerIP, "IP address of the TCP server")
	flag.Parse()

	log.Printf("Go TCP Client: Using server IP: %s:%s\n", *serverIP, config.TCPServerPort)

	// 1. Генерация тестового файла
	fileutils.GenerateTestFileIfNotExists(config.TestFileName, config.TotalFileSize)


	info, err := os.Stat(config.TestFileName)
	if err != nil || info.Size() != config.TotalFileSize {
		log.Fatalf("Go TCP Client: Test file '%s' issue. Size: %d (expected %d), Error: %v. Aborting.",
			config.TestFileName, info.Size(), config.TotalFileSize, err)
	}

	// 2. Инициализация агрегатора метрик
	metricsAggregator := metrics.NewMetricsAggregator("Go", config.TotalFileSize, config.ChunkSize)

	// 3. Установка соединения с сервером
	// net.Dial подключается к адресу по указанной сети ("tcp").
	// Возвращает net.Conn и ошибку.
	serverAddr := fmt.Sprintf("%s:%s", *serverIP, config.TCPServerPort)
	conn, err := net.DialTimeout("tcp", serverAddr, 10*time.Second) // Таймаут на подключение
	if err != nil {
		log.Fatalf("Go TCP Client: Failed to connect to server %s: %v\n", serverAddr, err)
	}
	defer conn.Close() // Гарантируем закрытие соединения.
	log.Printf("Go TCP Client: Connected to %s\n", serverAddr)

	 conn.SetReadDeadline(time.Now().Add(60 * time.Second)) // Пример: 60 сек на чтение
	 conn.SetWriteDeadline(time.Now().Add(60 * time.Second)) // Пример: 60 сек на запись

	// 4. Инициализация ChunkReader
	chunkReader, err := fileutils.NewChunkReader(config.TestFileName, config.ChunkSize)
	if err != nil {
		log.Fatalf("Go TCP Client: Failed to create ChunkReader for '%s': %v\n", config.TestFileName, err)
	}
	defer chunkReader.Close()

	// 5. Запуск таймера и отправка/получение чанков
	metricsAggregator.StartTimer()

	totalChunksToSend := chunkReader.TotalChunks()
	if chunkReader.FileSize() == 0 && totalChunksToSend == 0 {
		log.Println("Go TCP Client: Test file is empty. Nothing to send.")
	}

	for i := 0; ; i++ {
		chunkData, err := chunkReader.ReadNextChunk()
		if err != nil {
			if err == io.EOF { // Конец файла
				if len(chunkData) == 0 {
					log.Println("Go TCP Client: EOF reached, all data read from file.")
					break
				}
			} else {
				log.Printf("Go TCP Client: Error reading chunk %d from file: %v. Aborting.\n", i, err)
				metricsAggregator.StopTimer()
				metricsAggregator.PrintSummary()
				metricsAggregator.SaveToCSV()
				os.Exit(1)
			}
		}

		if len(chunkData) == 0 && chunkReader.EOF() {
			if i == 0 && chunkReader.FileSize() == 0 {
				break
			}
			if i > 0 && chunkReader.EOF() {
				break
			}
		}

		expectedReversedChunk := reversal.GetReversedBytes(chunkData)


		rttStartTime := time.Now()


		err = messaging.WriteMessage(conn, chunkData)
		if err != nil {
			log.Printf("Go TCP Client: Error writing chunk %d to server: %v. Aborting.\n", i, err)
			metricsAggregator.StopTimer()
			metricsAggregator.PrintSummary()
			metricsAggregator.SaveToCSV()
			os.Exit(1)
		}

		// Получаем ответ от сервера
		receivedData, err := messaging.ReadMessage(conn)
		if err != nil {
			log.Printf("Go TCP Client: Error reading response for chunk %d from server: %v. Aborting.\n", i, err)
			if err == io.EOF && i >= int(totalChunksToSend)-1 {
				log.Println("Go TCP Client: Server closed connection after all expected chunks, as expected.")
			}
			metricsAggregator.StopTimer()
			metricsAggregator.PrintSummary()
			metricsAggregator.SaveToCSV()
			os.Exit(1)
		}
		rtt := time.Since(rttStartTime)

		// Записываем метрики для чанка
		metricsAggregator.RecordChunkRTT(i, rtt)
		metricsAggregator.RecordChunkProcessed(len(chunkData)) // Размер исходного чанка

		// Верификация
		// bytes.Equal сравнивает два среза байт.
		verified := bytes.Equal(receivedData, expectedReversedChunk)
		metricsAggregator.RecordChunkVerified(verified)

		if !verified {
			log.Printf("Go TCP Client: ERROR! Chunk %d (original size: %d, received size: %d) verification FAILED.\n",
				i, len(chunkData), len(receivedData))
			// Остановка на первой ошибке
			metricsAggregator.StopTimer()
			metricsAggregator.PrintSummary()
			metricsAggregator.SaveToCSV()
			os.Exit(1)
		}
		 log.Printf("Go TCP Client: Chunk %d (size %d) sent, received, and verified OK. RTT: %s\n", i, len(chunkData), rtt)
		if chunkReader.EOF() && len(chunkData) > 0 { // Если это был последний чанк с данными
			log.Printf("Go TCP Client: Processed final chunk %d from file.\n", i)
			break // Выходим из цикла после обработки последнего чанка
		}
	}

	// 6. Остановка таймера и вывод результатов
	metricsAggregator.StopTimer()
	metricsAggregator.PrintSummary()
	err = metricsAggregator.SaveToCSV()
	if err != nil {
		log.Fatalf("Go TCP Client: Failed to save metrics to CSV: %v\n", err)
	}

	log.Println("Go TCP Client finished successfully.")
}
