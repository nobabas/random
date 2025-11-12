#pragma once

#include <UGVModule.h>

#define L 0.8		// Length of UGV in m
#define W 0.6		// Width of UGV in m

ref class CrashAvoidance : public UGVModule {
public:
	// Constructors
	CrashAvoidance(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser, SM_VehicleControl^ SM_VehicleControl);
	~CrashAvoidance();

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
	SM_Laser^ SM_Laser_;
	SM_VehicleControl^ SM_VehicleControl_;
	Stopwatch^ Watch;
};
