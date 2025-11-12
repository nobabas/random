#pragma once

#include <NetworkedModule.h>

ref class VehicleControl : NetworkedModule {
public:
	// Constructors
	VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl);
	~VehicleControl();

	// Derived from UGVModule
	error_state processSharedMemory() override;
	bool getShutdownFlag() override;
	void threadFunction() override;

	// Derived from NetworkedModule
	error_state connect(String^ hostName, int portNumber) override;
	error_state communicate() override;

	// Member functions
	error_state setupSharedMemory();
	error_state processHeartbeats();
	void shutdownThreads();
	error_state checkData();


private:
	SM_ThreadManagement^ SM_TM_;
	SM_VehicleControl^ SM_VehicleControl_;
	Stopwatch^ Watch;
	array<unsigned char>^ SendData;
	bool Watchdog;
};
