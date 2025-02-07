#pragma once
#include <inttypes.h>

namespace Config
{
	struct __attribute__((packed)) eeprom_body_t
	{
		// Подвеска
		struct
		{
			// Текущий режим
			uint8_t mode;

			// Текущее давление
			uint16_t target_pressure;

			// Предустановки давления
			uint16_t presets[3] = {500, 1000, 1500};
		} suspension;
	};
};
