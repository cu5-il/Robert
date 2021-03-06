#pragma once
#include <iostream>
#include <cmath>
#include <string>

#include "constants.h"
#include "myTypes.h"
//#include "myGlobals.h"
#include "A3200.h"
#include "A3200_functions.h"

#ifndef EXTRUSION_H
#define EXTRUSION_H

class Extruder {
private:
	bool _augerEnabled;
	bool _airEnabled;
	bool _enabled() { return _augerEnabled && _airEnabled; }
	A3200Handle _handle;
	TASKID _taskId;
public:
	Extruder()
	{
		_augerEnabled = false;
		_airEnabled = false;
		_handle = NULL;
		_taskId = TASKID_Library;
	}
	Extruder(A3200Handle handle, TASKID taskId);

	void enable();
	void disable(); 
	void set(double AO);
	void auger(bool enable = false);
	void air(bool enable = false);

};

inline Extruder::Extruder(A3200Handle handle, TASKID taskId) {
	_handle = handle;
	_taskId = taskId;
	//disable auger and air
	disable();
	if (!A3200IODigitalOutput(handle, _taskId, 0, AXISINDEX_00, 0)) { A3200Error(); } //equivalent to $WO[0].X = 0
	_augerEnabled = false;
	_airEnabled = false;
}
/**
 * @brief Enables extrusion by enabling the auger and air
*/
inline void Extruder::enable() {
	if (!A3200IODigitalOutput(_handle, _taskId, 0, AXISINDEX_00, 3)) { A3200Error(); } //equivalent to $WO[0].X = 3
	_augerEnabled = true;
	_airEnabled = true;
}
/**
 * @brief Disables extrusion by disabling the auger and air
*/
inline void Extruder::disable() {
	if (!A3200IODigitalOutput(_handle, _taskId, 0, AXISINDEX_00, 0)) { A3200Error(); } //equivalent to $WO[0].X = 0
	_augerEnabled = false;
	_airEnabled = false;
}
/**
 * @brief Sets the speed of the auger
 * @param AO Voltage output sent to the auger motor controller. Saturates at +/-10V
*/
inline void Extruder::set(double AO) {
	if (fabs(AO) > 10) { AO = copysign(10.0, AO); } // Saturate output at +/-10.0
	if (!A3200IOAnalogOutput(_handle, _taskId, 1, AXISINDEX_00, AO)) { A3200Error(); } //equivalent to $AO[1].X = AO
	
	if (AO < 0) {
		if (_airEnabled)
		air(false); } // disable air if retracting auger
	else if(!_enabled()) { enable(); }
}
/**
 * @brief Enables or disables the auger
 * @param enable Set to TRUE to enable the auger, FALSE to disable
*/
inline void Extruder::auger(bool enable) {
	if (enable) { // turn on the auger
		if (!A3200IODigitalOutputBit(_handle, _taskId, 1, AXISINDEX_00, 1)) { A3200Error(); } //equivalent to $DO[1].X = 1
		_augerEnabled = true;
	}
	else { // turn off the auger
		if (!A3200IODigitalOutputBit(_handle, _taskId, 1, AXISINDEX_00, 0)) { A3200Error(); } //equivalent to $DO[1].X = 0
		_augerEnabled = false;
	}
}
/**
 * @brief Enables or disables the air
 * @param enable Set to TRUE to enable the air, FALSE to disable
*/
inline void Extruder::air(bool enable) {
	if (enable) { // turn on the air
		if (!A3200IODigitalOutputBit(_handle, _taskId, 0, AXISINDEX_00, 1)) { A3200Error(); } //equivalent to $DO[0].X = 1
		_airEnabled = true;
	}
	else { // turn off the air
		if (!A3200IODigitalOutputBit(_handle, _taskId, 0, AXISINDEX_00, 0)) { A3200Error(); } //equivalent to $DO[0].X = 0
		_airEnabled = false;
	}
}

#endif // EXTRUSION_H
