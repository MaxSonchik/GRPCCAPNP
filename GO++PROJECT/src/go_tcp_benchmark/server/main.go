package main

import (
	"src/go_tcp_benchmark/pkg/config"
	"src/go_tcp_benchmark/pkg/messaging"
	"src/go_tcp_benchmark/pkg/reversal"
	"io"
	"log"
	"net"
)

func handleConnection(conn net.Conn) {
	// defer conn.Close() гарантирует, что соединение будет закрыто при выходе из функции,
	// независимо от того, как она завершается (нормально или из-за паники).
	defer conn.Close()
	remoteAddr := conn.RemoteAddr().String()
	log.Printf("Server: Accepted connection from %s\n", remoteAddr)

	// Используем bufio.Reader для эффективности, если это необходимо.
	// Однако, наш ReadMessage уже использует io.ReadFull, что тоже неплохо.
	// Для простоты, пока оставим как есть, но для очень высокопроизводительных
	// серверов можно рассмотреть bufio.NewReader(conn) и bufio.NewWriter(conn).

	for {
		// Читаем сообщение от клиента.
		receivedData, err := messaging.ReadMessage(conn)
		if err != nil {
			if err == io.EOF {
				log.Printf("Server: Client %s disconnected gracefully.\n", remoteAddr)
			} else {
				// Ошибки типа "connection reset by peer" или другие сетевые проблемы
				log.Printf("Server: Error reading from client %s: %v\n", remoteAddr, err)
			}
			return // Завершаем обработку этого соединения.
		}

		// log.Printf("Server: Received %d bytes from %s\n", len(receivedData), remoteAddr)

		// Реверсируем данные на месте.
		reversal.ReverseBytesInPlace(receivedData)

		// Отправляем реверсированные данные обратно клиенту.
		err = messaging.WriteMessage(conn, receivedData)
		if err != nil {
			log.Printf("Server: Error writing to client %s: %v\n", remoteAddr, err)
			return // Завершаем, если не можем отправить ответ.
		}
		// log.Printf("Server: Sent %d reversed bytes to %s\n", len(receivedData), remoteAddr)
	}
}

func main() {
	// net.Listen создает "слушателя" на указанном сетевом адресе и порту.
	// "tcp" - протокол, ":" + config.TCPServerPort - слушать на всех интерфейсах на этом порту.
	listener, err := net.Listen("tcp", ":"+config.TCPServerPort)
	if err != nil {
		log.Fatalf("Server: Failed to listen on port %s: %v\n", config.TCPServerPort, err)
	}
	// defer listener.Close() закроет слушателя при завершении main.
	defer listener.Close()
	log.Printf("Go TCP Server listening on port %s\n", config.TCPServerPort)

	// Бесконечный цикл для приема новых соединений.
	for {
		// listener.Accept() блокируется до тех пор, пока не появится новое входящее соединение.
		// Оно возвращает net.Conn для этого соединения и ошибку.
		conn, err := listener.Accept()
		if err != nil {
			// Если ошибка не временная (например, слушатель закрыт), выходим.
			// В реальном сервере здесь может быть более сложная обработка ошибок.
			log.Printf("Server: Failed to accept connection: %v\n", err)
			continue // Продолжаем пытаться принимать другие соединения
		}

		// Для обработки каждого клиента конкурентно, мы запускаем handleConnection в новой горутине.
		// Ключевое слово "go" запускает функцию в отдельной горутине.
		// Горутины - это легковесные потоки, управляемые средой выполнения Go.
		go handleConnection(conn)
	}
}
