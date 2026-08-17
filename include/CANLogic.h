#pragma once
#include <CANLibrary.h>
#include "CanObj/CanBlockInfo.hpp"
#include "CanObj/CanBlockCfg.hpp"
#include "CanObj/CanCtrlParam.hpp"
#include "CanObj/CanInfoParam.hpp"
#include "CANFunc.h"
#include <DrakePinD.hpp>

extern CAN_HandleTypeDef hcan;
extern bool HAL_CAN_Send(can_object_id_t id, uint8_t *data, uint8_t length);

namespace CANLib
{
	static constexpr uint8_t CFG_CANObjectsCount = 5;
	static constexpr uint16_t CAN_BASE_ID = 0x0240;
	
	DrakePinD can_rs({GPIOA, GPIO_PIN_15}, DrakePin::OutputOpenDrain, DrakePin::High);
	
	CANManager<CFG_CANObjectsCount> can_manager(&HAL_CAN_Send, &HAL_GetTick, &OnInterruptCtrl);
	
	CanBlockInfo obj_block_info(CAN_BASE_ID+0, OnStaticInfoReq, OnDynamicInfoReq);
	CanBlockCfg obj_block_cfg(CAN_BASE_ID+1, OnCfgSaveReset, block_cfg_table, block_cfg_table_count);

	CanCtrlParam<uint8_t> obj_suspension_mode(CAN_BASE_ID+4, Suspension::ChangeMode, Suspension::GetMode);
	CanCtrlParam<uint8_t> obj_suspension_value(CAN_BASE_ID+5, Suspension::ChangeValue, Suspension::GetValue);
	CanInfoParam<uint16_t> obj_suspension_pressure(CAN_BASE_ID+6, 500, &Suspension::pressure_value);

	void CAN_Enable()
	{
		HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE);
		HAL_CAN_Start(&hcan);
		
		can_rs.Off();
		
		return;
	}
	
	void CAN_Disable()
	{
		HAL_CAN_DeactivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF | CAN_IT_LAST_ERROR_CODE);
		HAL_CAN_Stop(&hcan);
		
		can_rs.On();
		
		return;
	}
	
	inline void Setup()
	{
		can_rs.Init();

		can_manager.AddObject(obj_block_info);
		can_manager.AddObject(obj_block_cfg);
		can_manager.AddObject(obj_suspension_mode);
		can_manager.AddObject(obj_suspension_value);
		can_manager.AddObject(obj_suspension_pressure);
		
		CAN_Enable();
		
		return;
	}

	inline void Loop(uint32_t &current_time)
	{
		can_manager.Processing();
		
		current_time = HAL_GetTick();
		return;
	}
}

IBlockInfoSender &BlockInfoSender = CANLib::obj_block_info;
