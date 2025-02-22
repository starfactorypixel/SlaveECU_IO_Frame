#pragma once
#include <CUtils.h>

namespace Suspension
{
	static constexpr uint16_t HYST_PRESSURE = 50;
	//static constexpr uint16_t COMP_TIMEOUT = 2 * 60;

	auto *CFG = &Config::config.data.suspension;


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
	//mode_t mode = MODE_OFF;
	//uint16_t pressure = 0;
	//uint16_t target_pressure = 0;
	uint16_t compressor_timeout = 0;

	MovingAverage<uint16_t, uint32_t, 5> pressure_average;



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
		CFG->mode = new_mode;
		
		switch(new_mode)
		{
			case MODE_OFF:
			{
				//target_pressure = CFG.pressure_target;
				
				break;
			}
			case MODE_PRESET_1:
			{
				CFG->pressure_target = CFG->presets[0];
				
				break;
			}
			case MODE_PRESET_2:
			{
				CFG->pressure_target = CFG->presets[1];
				
				break;
			}
			case MODE_PRESET_3:
			{
				CFG->pressure_target = CFG->presets[2];
				
				break;
			}
			case MODE_CUSTOM:
			{
				//target_pressure = CFG.pressure_target;
				
				break;
			}
			default:
			{
				break;
			}
		}

		CANLib::obj_suspension_mode.SetValue(0, CFG->mode, CAN_TIMER_TYPE_NONE, CAN_EVENT_TYPE_NORMAL);

		uint8_t tmp = map<uint16_t>(CFG->pressure_target, 0, CFG->presets[sizeofarray(CFG->presets)-1], 0, 255);
		CANLib::obj_suspension_value.SetValue(0, tmp, CAN_TIMER_TYPE_NONE, CAN_EVENT_TYPE_NORMAL);
		
		return;
	}

	void OnChangeValue(uint8_t value)
	{
		CFG->pressure_target = map<uint16_t>(value, 0, 255, 0, CFG->presets[sizeofarray(CFG->presets)-1]);

		//uint8_t tmp = map<uint16_t>(CFG->pressure_target, CFG->presets[0], CFG->presets[sizeofarray(CFG->presets)-1], 0, 255);
		CANLib::obj_suspension_value.SetValue(0, value, CAN_TIMER_TYPE_NONE, CAN_EVENT_TYPE_NORMAL);

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
			if(CFG->mode == MODE_CUSTOM)
			{
				OnChangeValue(can_frame.data[0]);
				
				can_frame.function_id = CAN_FUNC_EVENT_OK;
			}
			else
			{
				can_frame.function_id = CAN_FUNC_EVENT_ERROR;
			}
			
			return CAN_RESULT_CAN_FRAME;
		});

		OnChangeMode( (mode_t)CFG->mode );
		pressure_average.Set( CFG->pressure_target );
		
		return;
	}
	
	inline void Loop(uint32_t &current_time)
	{
		static uint32_t last_tick_pressure = 0;
		if(current_time - last_tick_pressure > 200)
		{
			last_tick_pressure = current_time;

			pressure_average.Push( GetPressure() );

			CANLib::obj_suspension_pressure.SetValue(0, pressure_average.Get(), CAN_TIMER_TYPE_NORMAL);
		}

		static uint32_t last_tick_logic = 0;
		if(current_time - last_tick_logic > 1000)
		{
			last_tick_logic = current_time;

			if(CFG->mode != MODE_OFF)
			{
				if( pressure_average.Get() > CFG->pressure_target + HYST_PRESSURE )
				{
					if(state != STATE_DRAINVALVE_ON)
					{
						CompressorCtrl(0);
						DrainValveCrtl(1);

						compressor_timeout = 0;
						state = STATE_DRAINVALVE_ON;
					}
				}
				else if( pressure_average.Get() + HYST_PRESSURE < CFG->pressure_target )
				{
					if(state != STATE_COMPRESSOR_ON)
					{
						CompressorCtrl(1);
						DrainValveCrtl(0);

						state = STATE_COMPRESSOR_ON;
					}
					else
					{
						if(++compressor_timeout == CFG->compressor_runtime)
						{
							CFG->mode = MODE_OFF;

							// CANEvent что компрессор работал долго и вырубился
						}
					}
				}
				else
				{
					if(state == STATE_COMPRESSOR_ON && pressure_average.Get() >= CFG->pressure_target)
					{
						CompressorCtrl(0);

						compressor_timeout = 0;
						state = STATE_NONE;
					}
					
					if(state == STATE_DRAINVALVE_ON && pressure_average.Get() <= CFG->pressure_target)
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
