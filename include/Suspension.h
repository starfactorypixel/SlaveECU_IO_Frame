#pragma once
#include <CUtils.h>

namespace Suspension
{
	static constexpr uint16_t HYST_PRESSURE = 50;
	static constexpr uint16_t COMP_TIMEOUT = 2 * 60;

	enum state_t : uint8_t
	{
		STATE_NONE = 0,
		STATE_COMPRESSOR_ON,
		STATE_COMPRESSOR_OFF,
		STATE_DRAINVALVE_ON,
		STATE_DRAINVALVE_OFF,
	};
	
	enum mode_t : uint8_t
	{
		MODE_OFF = 0,
		MODE_PRESET_1 = (1 << 0),
		MODE_PRESET_2 = (1 << 1),
		MODE_PRESET_3 = (1 << 2),
		MODE_CUSTOM = (1 << 7)
	};
	
	state_t state = STATE_NONE;
	mode_t mode = MODE_OFF;
	uint16_t pressure = 0;
	uint16_t target_pressure = 0;
	uint16_t compressor_timeout = 0;

	MovingAverage<uint16_t, uint32_t, 5> average_pressure;



	// Получить значение датчка давления
	uint16_t GetPressure()
	{
		return Analog::mux.adc_value[3];
	}
	
	// Управление компрессором
	void CompressorCtrl(uint8_t state)
	{
		Outputs::outObj.SetWrite(8, state);
		
		return;
	}

	// Управление спускным клапаном
	void DrainValveCrtl(uint8_t state)
	{
		Outputs::outObj.SetWrite(1, state);

		return;
	}

	void OnChangeMode(mode_t new_mode)
	{
		mode = new_mode;
		
		switch(new_mode)
		{
			case MODE_OFF:
			{
				target_pressure = Config::obj.body.suspension.target_pressure;
				
				break;
			}
			case MODE_PRESET_1:
			{
				target_pressure = Config::obj.body.suspension.presets[0];
				
				break;
			}
			case MODE_PRESET_2:
			{
				target_pressure = Config::obj.body.suspension.presets[1];
				
				break;
			}
			case MODE_PRESET_3:
			{
				target_pressure = Config::obj.body.suspension.presets[2];
				
				break;
			}
			case MODE_CUSTOM:
			{
				target_pressure = Config::obj.body.suspension.target_pressure;
				
				break;
			}
			default:
			{
				break;
			}
		}
		
		return;
	}
	
	
	
	inline void Setup()
	{
		CANLib::obj_suspension_mode.RegisterFunctionSet([](can_frame_t &can_frame, can_error_t &error) -> can_result_t
		{
			OnChangeMode( (mode_t)can_frame.data[0] );
			
			can_frame.function_id = CAN_FUNC_EVENT_OK;
			return CAN_RESULT_CAN_FRAME;
		});
		
		CANLib::obj_suspension_value.RegisterFunctionSet([](can_frame_t &can_frame, can_error_t &error) -> can_result_t
		{
			if(mode == MODE_CUSTOM)
			{
				target_pressure = (can_frame.data[0] || (uint16_t)(can_frame.data[1] << 8));

				can_frame.function_id = CAN_FUNC_EVENT_OK;
			}
			else
			{
				can_frame.function_id = CAN_FUNC_EVENT_ERROR;
			}
			
			return CAN_RESULT_CAN_FRAME;
		});

		OnChangeMode( (mode_t)Config::obj.body.suspension.mode );
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		static uint32_t last_tick_pressure = 0;
		if(current_time - last_tick_pressure > 200)
		{
			last_tick_pressure = current_time;

			average_pressure.Push( GetPressure() );

			CANLib::obj_suspension_pressure.SetValue(0, average_pressure.Get(), CAN_TIMER_TYPE_NORMAL);
		}

		static uint32_t last_tick_logic = 0;
		if(current_time - last_tick_logic > 1000)
		{
			last_tick_logic = current_time;

			if(mode != MODE_OFF)
			{
				if( average_pressure.Get() > target_pressure + HYST_PRESSURE )
				{
					if(state != STATE_DRAINVALVE_ON)
					{
						CompressorCtrl(0);
						DrainValveCrtl(1);

						compressor_timeout = 0;
						state = STATE_DRAINVALVE_ON;
					}
				}
				else if( average_pressure.Get() + HYST_PRESSURE < target_pressure )
				{
					if(state != STATE_COMPRESSOR_ON)
					{
						CompressorCtrl(1);
						DrainValveCrtl(0);

						state = STATE_COMPRESSOR_ON;
					}
					else
					{
						if(++compressor_timeout == COMP_TIMEOUT)
						{
							mode = MODE_OFF;

							// CANEvent что компрессор работал долго и вырубился
						}
					}
				}
				else
				{
					if(state == STATE_COMPRESSOR_ON && average_pressure.Get() >= target_pressure)
					{
						CompressorCtrl(0);

						compressor_timeout = 0;
						state = STATE_NONE;
					}
					
					if(state == STATE_DRAINVALVE_ON && average_pressure.Get() <= target_pressure)
					{
						DrainValveCrtl(0);

						state = STATE_NONE;
					}
				}
			}
			else
			{
				if(state == STATE_COMPRESSOR_ON)
					CompressorCtrl(0);
				
				if(state == STATE_DRAINVALVE_ON)
					DrainValveCrtl(0);

				state = STATE_NONE;
			}
		}
		
		current_time = HAL_GetTick();
		
		return;
	}
};
