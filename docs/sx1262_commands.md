# SX1261/2 Commands (Pages 66-68)

## 11.1 Operational Modes Commands

| Command | Opcode | Parameters | Description |
|---------|--------|------------|-------------|
| SetSleep | 0x84 | sleepConfig | Set Chip in SLEEP mode |
| SetStandby | 0x80 | standbyConfig | Set Chip in STDBY_RC or STDBY_XOSC mode |
| SetFs | 0xC1 | - | Set Chip in Frequency Synthesis mode |
| SetTx | 0x83 | timeout[23:0] | Set Chip in Tx mode |
| SetRx | 0x82 | timeout[23:0] | Set Chip in Rx mode |
| StopTimerOnPreamble | 0x9F | StopOnPreambleParam | Stop Rx timeout on Sync Word/Header or preamble detection |
| SetRxDutyCycle | 0x94 | rxPeriod[23:0], sleepPeriod[23:0] | Store values of RTC setup for listen mode and if period parameter is not 0, set chip into RX mode |
| SetCad | 0xC5 | - | Set chip in RX mode with passed CAD parameters |
| SetTxContinuousWave | 0xD1 | - | Set chip in TX mode with infinite carrier wave settings |
| SetTxInfinitePreamble | 0xD2 | - | Set chip into TX mode with infinite preamble settings |
| SetRegulatorMode | 0x96 | regModeParam | Select LDO or DC_DC+LDO for CFG_XOSC, FS, RX or TX mode |
| Calibrate | 0x89 | calibParam | Calibrate the RC13, RC64, ADC, PLL, Image according to parameter |
| CalibrateImage | 0x98 | freq1, freq2 | Launches image calibration at the given frequencies |
| SetPaConfig | 0x95 | paDutyCycle, HpMax, deviceSel, paLUT | Configure Duty Cycle, Max output power, device for the PA for SX1261 or SX1262 |
| SetRxTxFallbackMode | 0x93 | fallbackMode | Defines into which mode the chip goes after a TX / RX done |

## 11.2 Register and Buffer Access Commands

| Command | Opcode | Parameters | Description |
|---------|--------|------------|-------------|
| WriteRegister | 0x0D | address[15:0], data[0:n] | Write into one or several registers |
| ReadRegister | 0x1D | address[15:0] | Read one or several registers |
| WriteBuffer | 0x0E | offset, data[0:n] | Write data into the FIFO |
| ReadBuffer | 0x1E | offset | Read data from the FIFO |

## 11.3 DIO and IRQ Control

| Command | Opcode | Parameters | Description |
|---------|--------|------------|-------------|
| SetDioIrqParams | 0x08 | IrqMask[15:0], Dio1Mask[15:0], Dio2Mask[15:0], Dio3Mask[15:0] | Configure the IRQ and the DIOs attached to each IRQ |
| GetIrqStatus | 0x12 | - | Get the values of the triggered IRQs |
| ClearIrqStatus | 0x02 | - | Clear one or several of the IRQs |
| SetDIO2AsRfSwitchCtrl | 0x9D | enable | Configure radio to control an RF switch from DIO2 |
| SetDIO3AsTcxoCtrl | 0x97 | tcxoVoltage, timeout[23:0] | Configure the radio to use a TCXO controlled by DIO3 |

## 11.4 RF, Modulation and Packet Commands

| Command | Opcode | Parameters | Description |
|---------|--------|------------|-------------|
| SetRfFrequency | 0x86 | rfFreq[31:0] | Set the RF frequency of the radio |
| SetPacketType | 0x8A | protocol | Select the packet type corresponding to the modem |
| GetPacketType | 0x11 | - | Get the current packet configuration for the device |
| SetTxParams | 0x8E | power, rampTime | Set output power and ramp time for the PA |
| SetModulationParams | 0x8B | modParam1, modParam2, modParam3 | Compute and set values in selected protocol modem for given modulation parameters |
| SetPacketParams | 0x8C | packetParam1, packetParam2, packetParam3, packetParam4, packetParam5, packetParam6, packetParam7, packetParam8, packetParam9 | Set values on selected protocol modem for given packet parameters |
| SetCadParams | 0x88 | cadSymbolNum, cadDetPeak, cadDetMin, cadExitMode, cadTimeout | Set the parameters which are used for performing a CAD (LoRa only) |
| SetBufferBaseAddress | 0x8F | TxbaseAddr, RxbaseAddr | Store TX and RX base address in register of selected protocol modem |
| SetLoRaSymbNumTimeout | 0xA0 | SymbNum | Set the number of symbol the modem has to wait to validate a lock |

## 11.5 Status Commands

| Command | Opcode | Parameters | Description |
|---------|--------|------------|-------------|
| GetStatus | 0xC0 | - | Returns the current status of the device |
| GetRssiInst | 0x15 | - | Returns the instantaneous measured RSSI while in Rx mode |
| GetRxBufferStatus | 0x13 | - | Returns PaylaodLengthRx(7:0), RxBufferPointer(7:0) |
| GetPacketStatus | 0x14 | - | Returns RssiAvg, RssiSync, PStatus2, PStatus3, PStatus4 in FSK protocol, returns RssiPkt, SnrPkt in LoRa protocol |
| GetDeviceErrors | 0x17 | - | Returns the error which has occurred in the device |
| ClearDeviceErrors | 0x07 | 0x00 | Clear all the error(s). The error(s) cannot be cleared independently |
| GetStats | 0x10 | - | Returns statistics on the last few received packets |
| ResetStats | 0x00 | - | Resets the value read by the command GetStats |
