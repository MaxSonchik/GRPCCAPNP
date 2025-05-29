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

	defer conn.Close()
	remoteAddr := conn.RemoteAddr().String()
	log.Printf("Server: Accepted connection from %s\n", remoteAddr)

	for {
		receivedData, err := messaging.ReadMessage(conn)
		if err != nil {
			if err == io.EOF {
				log.Printf("Server: Client %s disconnected gracefully.\n", remoteAddr)
			} else {
				log.Printf("Server: Error reading from client %s: %v\n", remoteAddr, err)
			}
			return
		}

		log.Printf("Server: Received %d bytes from %s\n", len(receivedData), remoteAddr)

		reversal.ReverseBytesInPlace(receivedData)

		err = messaging.WriteMessage(conn, receivedData)
		if err != nil {
			log.Printf("Server: Error writing to client %s: %v\n", remoteAddr, err)
			return
		}
		log.Printf("Server: Sent %d reversed bytes to %s\n", len(receivedData), remoteAddr)
	}
}

func main() {
	// "tcp" - протокол, ":" + config.TCPServerPort - слушать на всех интерфейсах на этом порту.
	listener, err := net.Listen("tcp", ":"+config.TCPServerPort)
	if err != nil {
		log.Fatalf("Server: Failed to listen on port %s: %v\n", config.TCPServerPort, err)
	}
	defer listener.Close()
	log.Printf("Go TCP Server listening on port %s\n", config.TCPServerPort)


	for {

		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Server: Failed to accept connection: %v\n", err)
			continue
		}

		//Горутин для параллельного выполнения
		go handleConnection(conn)
	}
}
