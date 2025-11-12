#pragma once

#include <UGVModule.h>

#define MAX_SPEED 1			// in meter per sec
#define MAX_STEERING 40		// in degree

ref class Controller : public UGVModule {
public:
	// Constructors
	Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl);
	~Controller();

	// Derived from UGVModule
	error_state processSharedMemory() override;
	bool getShutdownFlag() override;
	void threadFunction() override;

	// Member functions
	error_state setupSharedMemory();
	error_state processHeartbeats();
	void shutdownThreads();
	error_state checkData();

private:
	SM_ThreadManagement^ SM_TM_;
	SM_VehicleControl^ SM_VehicleControl_;
	Stopwatch^ Watch;
	ControllerInterface* xBox_;
};
