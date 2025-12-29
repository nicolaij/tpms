/* 
*	Part ESPHome project https://github.com/nicolaij/tpms
*   decode and encode TPMS RF data for Geely Coolray 2018- (SX11)
*/

#include <cstdint>
#ifndef __TPMS_H__
#define __TPMS_H__

namespace TPMS
{
#define POLYNOMIAL (0x1070U << 3)

	struct tpms
	{
		union
		{
			struct
			{
				uint8_t id[4];
				uint8_t pressure;
				uint8_t temperature;
				uint8_t flags;
				uint8_t crc;
			};
			uint8_t data[8];
		};
	};

	typedef union
	{
		uint32_t integer;
		uint8_t bytes[4];
	} int_bytes_union;

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

	uint8_t demanchester(char *dest, const char *src, int rezbits)
	{
		int i = 0;
		char *s = (char *)src;
		char *d = dest;
		int cnt = 0;
		char m2 = 0;
		*d = 0;
		while (i < rezbits)
		{
			int dpos = 7 - (i % 8);
			int spos = 7 - (((i % 8) * 2) % 8) - 1;
			if (((*s >> spos) & 0b11) == 0b10)
			{
				*d = *d | (0b1 << dpos);
			}
			else if (((*s >> spos) & 0b11) == 0b01)
			{
				*d = *d & ~(0b1 << dpos);
			}
			else
			{
				// manchester error
				break;
			}
			i++;

			if (dpos == 0)
			{
				d++;
				cnt++;
				*d = 0;
			}

			if (spos == 0)
				s++;
		};
		return cnt;
	};

	uint8_t manchester(char *dest, const char *src, int bits)
	{
		int i = 0;
		char *s = (char *)src;
		char *d = dest;
		int cnt = 0;
		char m2 = 0;
		while (i < bits)
		{
			int spos = 7 - (i % 8);
			int dpos = 7 - (((i % 8) * 2) % 8) - 1;
			*d = *d & ~(0b11 << dpos);
			if (*s & (0b1 << spos))
			{
				*d = *d | (0b10 << dpos);
			}
			else
			{
				*d = *d | (0b01 << dpos);
			}

			i++;

			if (dpos == 0)
			{
				d++;
				cnt++;
			}

			if (spos == 0)
				s++;
		}
		return cnt;
	}

#define to_kpa(x) ((int)x * 7 * 2 / 10)
#define to_raw(x) ((int)x * 10 / (7 * 2))

	int tpms_coolray(char *data, tpms td)
	{
		td.crc = Crc8Block(0, td.data, 7);
		return manchester(data, (const char *)td.data, 8 * 8);
	}

	int tpms_coolray(char *data, const uint8_t *id, int pressure, int temperature, int flags = 0x40)
	{
		tpms td;
		td.id[0] = id[0];
		td.id[1] = id[1];
		td.id[2] = id[2];
		td.id[3] = id[3];

		td.pressure = (uint8_t)to_raw(pressure);
		td.temperature = (uint8_t)temperature + 50;
		td.flags = flags;

		return TPMS::tpms_coolray(data, td);
	}
}

#endif //__TPMS_H__
