#pragma once
#include <inttypes.h>

namespace Config
{
	struct __attribute__((packed)) app_config_t
	{
		// Подвеска
		struct
		{
			// Текущий режим
			uint8_t mode = 0;

			// Настроенное (целевое) давление
			uint16_t pressure_target = 0;

			// Предустановки давления
			uint16_t presets[3] = {500, 1000, 1500};

			// Максимальное время работы компрессора
			uint16_t compressor_runtime = 120;
		} suspension;
		
		uint8_t test[50]; uint8_t test2[16];
	};
};
