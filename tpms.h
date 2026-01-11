/* Parce RF data TPMS Geely Coolray 2018- (SX11)
 *  2025 nicj@tut.by
 */
#include <cstdint>
#ifndef __TPMS_H__
#define __TPMS_H__

namespace TPMS
{
	struct tpms
	{
		union
		{
			struct
			{
				uint8_t id[4];
				uint8_t pressure;
				int8_t temperature;
				uint8_t flags;
				uint8_t crc;
			};
			uint8_t data[8];
		};
	};

	/**
	 * Декодирование Манчестерского кода (G.E. Thomas: 10 -> 1, 01 -> 0).
	 *
	 * @param dest         Указатель на буфер для восстановленных данных
	 * @param src          Указатель на закодированные данные (размер byte_number * 2)
	 * @param byte_number  Количество байт, которые должны получиться на выходе
	 * @return             0 в случае успеха, -1 если обнаружена ошибка в коде
	 */
	int decode_manchester_ge_thomas(char *dest, const char *src, int byte_number)
	{
		for (int i = 0; i < byte_number; i++)
		{
			// Читаем два байта из src, которые содержат один исходный байт
			uint16_t encoded_word = (unsigned char)src[2 * i + 1] | ((unsigned char)src[2 * i + 0] << 8);
			uint8_t decoded_byte = 0;

			for (int bit = 0; bit < 8; bit++)
			{
				// Извлекаем пару бит, соответствующих одному исходному биту
				// bit_pair будет принимать значения: 1 (01), 2 (10), 0 (00) или 3 (11)
				int bit_pair = (encoded_word >> (2 * bit)) & 0x03;

				if (bit_pair == 0x02)
				{ // Биты "10"
					decoded_byte |= (1 << bit);
				}
				else if (bit_pair == 0x01)
				{ // Биты "01"
				  // Это логический 0, ничего не делаем (бит уже 0)
				}
				else
				{
					// Ошибка: комбинации 00 или 11 недопустимы в середине такта
					return -1;
				}
			}
			dest[i] = (char)decoded_byte;
		}
		return 0;
	}

	/**
	 * Кодирование Манчестер (G.E. Thomas: 1 -> 10, 0 -> 01).
	 *
	 * @param dest         Указатель на выходной буфер (размер >= byte_number * 2)
	 * @param src          Указатель на исходные данные
	 * @param byte_number  Количество исходных байт
	 */
	void encode_manchester_ge_thomas(char *dest, const char *src, int byte_number)
	{
		for (int i = 0; i < byte_number; i++)
		{
			unsigned char current_byte = (unsigned char)src[i];
			unsigned short encoded_word = 0;

			// Кодируем каждый из 8 бит байта
			for (int bit = 0; bit < 8; bit++)
			{
				// Извлекаем текущий бит (LSB-first)
				if ((current_byte >> bit) & 0x01)
				{
					// Логическая '1' -> 10
					// В паре из двух бит: первый (2*bit) = 0, второй (2*bit+1) = 1
					// (При последовательной передаче сначала уйдет бит 2*bit, затем 2*bit+1)
					encoded_word |= (1 << (2 * bit + 1));
				}
				else
				{
					// Логический '0' -> 01
					// В паре из двух бит: первый (2*bit) = 1, второй (2*bit+1) = 0
					encoded_word |= (1 << (2 * bit));
				}
			}

			// Записываем 16-битный результат в два последовательных байта dest
			dest[2 * i + 1] = (char)(encoded_word & 0xFF);
			dest[2 * i + 0] = (char)((encoded_word >> 8) & 0xFF);
		}
	}

#define POLYNOMIAL (0x1070U << 3)

	unsigned char Crc8(unsigned char inCrc, unsigned char inData)
	{
		int i;
		uint16_t data;

		data = inCrc ^ inData;
		data <<= 8;

		for (i = 0; i < 8; i++)
		{
			if ((data & 0x8000) != 0)
			{
				data = data ^ POLYNOMIAL;
			}
			data = data << 1;
		}

		return (unsigned char)(data >> 8);
	} // Crc8

	uint8_t Crc8Block(uint8_t crc, uint8_t *data, uint8_t len)
	{
		while (len > 0)
		{
			crc = Crc8(crc, *data++);
			len--;
		}

		return crc;
	} // Crc8Block

#define to_kpa(x) (((int)x * 10 * (7 * 2) / 10 + 5) / 10)
#define to_raw(x) ((((int)x * 10 * 10) / (7 * 2) + 5) / 10)

	int tpms_coolray(tpms *td, const uint8_t *id, int pressure, int temperature, int flags = 0x40)
	{
		td->id[0] = id[0];
		td->id[1] = id[1];
		td->id[2] = id[2];
		td->id[3] = id[3];

		td->pressure = (uint8_t)to_raw(pressure);
		td->temperature = (uint8_t)temperature + 50;
		td->flags = flags;

		td->crc = Crc8Block(0, td->data, 7);

		return 0;
	}
}

#endif //__TPMS_H__
