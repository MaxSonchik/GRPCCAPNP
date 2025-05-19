// Package reversal предоставляет утилиты для реверсирования срезов байт.
package reversal

// ReverseBytesInPlace реверсирует содержимое среза байт на месте.
// Срезы (slices) в Go - это гибкие представления массивов.
// []byte - это срез байт, основной тип для работы с бинарными данными.
func ReverseBytesInPlace(data []byte) {
	for i, j := 0, len(data)-1; i < j; i, j = i+1, j-1 {
		data[i], data[j] = data[j], data[i] // Параллельное присваивание в Go
	}
}

// GetReversedBytes создает и возвращает новый срез с реверсированным содержимым.
// Это аналог get_reversed_vector_content из C++.
func GetReversedBytes(data []byte) []byte {
	// Создаем новый срез такой же длины и емкости, как и исходный.
	reversed := make([]byte, len(data))
	// Копируем данные из исходного среза в новый.
	// Функция copy возвращает количество скопированных байт.
	copy(reversed, data)
	// Реверсируем новый срез на месте.
	ReverseBytesInPlace(reversed)
	return reversed
}
