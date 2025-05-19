// Package messaging обрабатывает кадрирование сообщений (длина + данные) для TCP.
package messaging

import (
	"encoding/binary" // Для преобразования чисел в байты и обратно
	"io"              // Для io.ReadFull и io.EOF
	"net"             // Для net.Conn
)

const HeaderSize = 4 // Размер заголовка (uint32 для длины)

// WriteMessage отправляет сообщение (длина + полезная нагрузка) в net.Conn.
// net.Conn - это интерфейс, представляющий сетевое соединение.
func WriteMessage(conn net.Conn, payload []byte) error {
	// Создаем буфер для заголовка.
	header := make([]byte, HeaderSize)
	// Записываем длину полезной нагрузки в заголовок в сетевом порядке байт (BigEndian).
	// binary.BigEndian - это объект, реализующий интерфейс ByteOrder.
	binary.BigEndian.PutUint32(header, uint32(len(payload)))

	// Отправляем заголовок.
	// conn.Write пытается записать len(header) байт из header.
	// Он возвращает количество записанных байт и ошибку.
	if _, err := conn.Write(header); err != nil {
		return err // Возвращаем ошибку, если запись заголовка не удалась.
	}

	// Отправляем полезную нагрузку.
	if len(payload) > 0 { // Отправляем тело, только если оно не пустое
		if _, err := conn.Write(payload); err != nil {
			return err // Возвращаем ошибку, если запись полезной нагрузки не удалась.
		}
	}
	return nil // Успешная отправка.
}

// ReadMessage читает сообщение (длина + полезная нагрузка) из net.Conn.
func ReadMessage(conn net.Conn) ([]byte, error) {
	header := make([]byte, HeaderSize)
	// io.ReadFull читает ровно len(header) байт из conn в header.
	// Если EOF встречается до того, как прочитано len(header) байт,
	// ReadFull возвращает io.ErrUnexpectedEOF.
	if _, err := io.ReadFull(conn, header); err != nil {
		return nil, err // Включая io.EOF, если соединение закрыто до чтения заголовка.
	}

	// Парсим длину из заголовка.
	bodyLength := binary.BigEndian.Uint32(header)

	// Если длина тела 0, возвращаем пустой срез.
	if bodyLength == 0 {
		return []byte{}, nil
	}

	// Читаем тело сообщения.
	body := make([]byte, bodyLength)
	if _, err := io.ReadFull(conn, body); err != nil {
		return nil, err // Может быть io.ErrUnexpectedEOF, если соединение закрыто во время чтения тела.
	}
	return body, nil
}
