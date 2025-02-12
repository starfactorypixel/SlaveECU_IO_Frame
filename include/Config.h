#pragma once
#include <inttypes.h>
#include <CUtils.h>
#include "ConfigData.h"

//extern CRC_HandleTypeDef hcrc;

namespace Config
{
	static constexpr uint16_t EEPROM_OFFSET_MAIN = 0;
	static constexpr uint16_t DATA_SIZE = 256;
	static constexpr uint16_t DATA_H_SIZE = 9;
	static constexpr uint16_t DATA_PAGE_SIZE = SPI::eeprom.EEPROM_PAGE_SIZE;
	static constexpr uint16_t EEPROM_OFFSET_MIRROR = DATA_SIZE + EEPROM_OFFSET_MAIN;
	static constexpr uint32_t EEPROM_SAVE_INTERVAL = 30 * 1000;
	static constexpr uint32_t MIRROR_TIME_SYNC = 10 * 60 * 1000;
/*
	static_assert(EEPROM_OFFSET_MAIN % DATA_PAGE_SIZE == 0, "EEPROM_OFFSET_MAIN must be a multiple of 32!");
	
	// Общая структура всего блока данных
	struct __attribute__((packed)) eeprom_t
	{
		// Версия формата заголовка, а так-же флаг наличия записи в блоке (если 0x00 или 0xFF, то считаем что блок не инициализирован)
		uint8_t verison = 0x02;
		
		// Счётчик записей в память
		uint32_t counter = 0x00000001;
		
		// Блок полезных данных
		app_config_t body;
		
		// Заполнитель пустого места в объекте
		uint8_t _reserved[ (DATA_SIZE - DATA_H_SIZE - sizeof(app_config_t)) ];
		
		// Контрольная сумма всей структуры
		uint32_t crc32;
	} obj;
	static_assert(sizeof(eeprom_t) == DATA_SIZE, "Structures should have the same size!");
*/
	

















	// Структура - формат хранения данных в EEPROM, постранично
	struct __attribute__((packed)) eeprom_page_t
	{
		// 1 байт - Версия
		uint8_t version = 0x03;

		// 3 байта - Счётчик записей
		uint32_t counter : 24;

		// X байт - Полезная нагрузка
		uint8_t payload[ (DATA_PAGE_SIZE - 6) ];

		// 2 байта - Контрольная сумма
		uint16_t crc;
	};
	static_assert(sizeof(eeprom_page_t) == DATA_PAGE_SIZE, "The structure must be of size DATA_PAGE_SIZE!");
	
	static constexpr uint16_t PAYLOAD_SIZE = sizeof(eeprom_page_t::payload);
	static constexpr uint16_t DATA_LENGTH = sizeof(eeprom_page_t) - 2;
	
	// Структура, которая позволяет выровнять config_t кратно payload
	struct __attribute__((packed)) config_t
	{
		// Объект настроек
		app_config_t data;
		
		// Массив для автоматического выравнивания
		uint8_t _fill[ ((PAYLOAD_SIZE - (sizeof(app_config_t) % PAYLOAD_SIZE)) % PAYLOAD_SIZE) ];
	} config;
	static_assert(sizeof(config_t) % PAYLOAD_SIZE == 0, "The structure must have a size that is a multiple of PAYLOAD_SIZE!");
	
	
	
	// Загружает idx страницу EEPROM в объект page
	// Возвращает true если версия и crc верные
	bool LoadPage2(uint8_t idx, const eeprom_page_t &page)
	{
		bool result = false;
		
		SPI::eeprom.ReadPage(idx, ((uint8_t *) &page));
		if(page.version > 0x00 && page.version < 0xFF)
		{
			if(CRC16_XModem( ((uint8_t *) &page), DATA_LENGTH ) == page.crc)
			{
				result = true;
			}
		}
		
		return result;
	}
	
	// Сохраняет idx страницу EEPROM как есть
	bool WritePage2(uint8_t idx, const eeprom_page_t &page)
	{
		bool result = false;

		SPI::eeprom.WritePage(idx, ((uint8_t *) &page));
		result = true;
		
		return result;
	}
	
	// Инициализирует idx страницу EEPROM
	// Требует на входе page.payload
	bool InitPage2(uint8_t idx, eeprom_page_t &page)
	{
		bool result = false;
		
		page.version = 0x03;				// !!!!!!!!!!!!!!!!!!
		page.counter = 1;
		page.crc = CRC16_XModem( ((uint8_t *) &page), DATA_LENGTH );
		result = WritePage2(idx, page);
		
		return result;
	}
	
	// Обновляет idx страницу EEPROM
	bool UpdatePage2(uint8_t idx, const eeprom_page_t &page)
	{
		bool result = false;

		DEBUG_LOG_TOPIC("EEUpdate", "Enter\n");
		
		eeprom_page_t page_in_ee = {};
		if(LoadPage2(idx, page_in_ee) == true)
		{
			if( memcmp( ((uint8_t *) &page_in_ee.payload), ((uint8_t *) &page.payload), PAYLOAD_SIZE ) != 0 )
			{
				memcpy( ((uint8_t *) &page_in_ee.payload), ((uint8_t *) &page.payload), PAYLOAD_SIZE );
				page_in_ee.counter++;
				page_in_ee.crc = CRC16_XModem( ((uint8_t *) &page_in_ee), DATA_LENGTH );
				
				result = WritePage2(idx, page_in_ee);

				DEBUG_LOG_TOPIC("EEUpdate", "Write!\n");
			}
			else
			{
				result = true;
			}
		}
		// А если не успешное чтение?

		DEBUG_LOG_TOPIC("EEUpdate", "Exit\n");
		
		return result;
	}
	
	// Читает конфиг EEPROM
	void LoadConfig2(uint8_t eeprom_offset_page)
	{
		uint16_t payload_length = sizeof(config_t);
		uint8_t  payload_count = (sizeof(config_t) + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;
		uint16_t payload_offset = 0;
		
		eeprom_page_t page = {};
		for(uint8_t idx = 0; idx < payload_count; ++idx)
		{
			if(LoadPage2((idx + eeprom_offset_page), page) == true)
			{
				memcpy( ((uint8_t *) &config) + payload_offset, ((uint8_t *) &page.payload), PAYLOAD_SIZE );
			}
			else
			{
				DEBUG_LOG_TOPIC("EELoad", "Load error, page: %d, data:\n", (idx + eeprom_offset_page));
				DEBUG_LOG_ARRAY_HEX("EE", ((uint8_t *) &page), sizeof(page));
				DEBUG_LOG_NEW_LINE();

				memcpy( ((uint8_t *) &page.payload), ((uint8_t *) &config) + payload_offset, PAYLOAD_SIZE );
				InitPage2((idx + eeprom_offset_page), page);
			}
			
			payload_offset += PAYLOAD_SIZE;
		}
	}
	
	// Пишет конфиг EEPROM
	void SaveConfig2(uint8_t eeprom_offset_page)
	{
		uint16_t payload_length = sizeof(config_t);
		uint8_t  payload_count = (sizeof(config_t) + PAYLOAD_SIZE - 1) / PAYLOAD_SIZE;
		uint16_t payload_offset = 0;
		
		eeprom_page_t page = {};
		for(uint8_t idx = 0; idx < payload_count; ++idx)
		{
			memcpy( ((uint8_t *) &page.payload), ((uint8_t *) &config) + payload_offset, PAYLOAD_SIZE );
			if(UpdatePage2((idx + eeprom_offset_page), page) == true)
			{
				// OK
			}
			else
			{
				DEBUG_LOG_TOPIC("EESave", "Load error, page: %d, data:\n", (idx + eeprom_offset_page));
				DEBUG_LOG_ARRAY_HEX("EE", ((uint8_t *) &page), sizeof(page));
				DEBUG_LOG_NEW_LINE();
			}
			
			payload_offset += PAYLOAD_SIZE;
		}
	}
	
	
	inline void Setup()
	{
		for (uint8_t i = 0; i < sizeof(config.data.test); i++)
		{
			config.data.test[i] = (0x80 + i);
		}
		



		LoadConfig2(EEPROM_OFFSET_MAIN);

		DEBUG_LOG_ARRAY_HEX("EESetup", ((uint8_t *) &config), sizeof(config));
		DEBUG_LOG_NEW_LINE();
		
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		static uint32_t last_save_time = 0;
		if(current_time - last_save_time > EEPROM_SAVE_INTERVAL)
		{
			last_save_time = current_time;
			
			SaveConfig2(EEPROM_OFFSET_MAIN);
		}
		
		// При выходе обновляем время
		current_time = HAL_GetTick();
		
		return;
	}
};
