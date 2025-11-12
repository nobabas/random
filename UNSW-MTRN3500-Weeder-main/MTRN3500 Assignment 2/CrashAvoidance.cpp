#include "CrashAvoidance.h"

// Constructors
CrashAvoidance::CrashAvoidance(SM_ThreadManagement^ SM_TM, SM_Laser^ SM_Laser, SM_VehicleControl^ SM_VehicleControl) {
	SM_TM_ = SM_TM;
	SM_Laser_ = SM_Laser;
	SM_VehicleControl_ = SM_VehicleControl;
	Watch = gcnew Stopwatch;
}

CrashAvoidance::~CrashAvoidance() {

}

// Derived from UGVModule
error_state CrashAvoidance::processSharedMemory() {

	// Request Vehicle Control Mutex
	Monitor::Enter(SM_VehicleControl_->lockObject);

	SM_VehicleControl_->StopFlag = false;
	double delta = SM_VehicleControl_->Steering;

	// Release Vehicle Control Mutex
	Monitor::Exit(SM_VehicleControl_->lockObject);

	// First calculate deltaL and deltaR
	double deltaL;
	double deltaR;
	if (delta == 0) {
		deltaL = 0;
		deltaR = 0;
	}
	else {
		double R = L / Math::Tan(delta * Math::PI / 180.0);
		deltaL = Math::Atan2(L, (R + W / 2));
		deltaR = Math::Atan2(L, (R - W / 2));
	}

	// Request Laser Mutex
	Monitor::Enter(SM_Laser_->lockObject);

	for (int i = 0; i < STANDARD_LASER_LENGTH; i++) {
		double xData = SM_Laser_->x[i] / 1000.0;
		double yData = SM_Laser_->y[i] / 1000.0;
		
		// Ignore data which is 0, 0 
		if (xData == 0.0 && yData == 0.0) continue;

		// Check if object is within 1m of the laser, if no, next iteration
		if (Math::Sqrt(pow(xData, 2) + pow(yData, 2)) > 1) continue;

		// Check if object is within the path of UGV, if no, next iteration
		double upperY = xData * Math::Tan(deltaL) + W / 2;
		double lowerY = xData * Math::Tan(deltaR) - W / 2;
		if (yData > upperY || yData < lowerY) continue;

		// If both within 1m of laser and within the path of UGV, stop UGV
		Monitor::Enter(SM_VehicleControl_->lockObject);
		SM_VehicleControl_->StopFlag = true;
		Monitor::Exit(SM_VehicleControl_->lockObject);
		break;
	}

	Console::Write("OBJECT IN FRONT RESULT IS ");
	Console::WriteLine(SM_VehicleControl_->StopFlag);

	// Release Laser Mutex
	Monitor::Exit(SM_Laser_->lockObject);

	return SUCCESS;
}

bool CrashAvoidance::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_CRASHAVOIDANCE);
}

void CrashAvoidance::threadFunction() {
	Console::WriteLine("Crash Avoidance thread is starting.");
	// Setup the stopwatch
	Watch = gcnew Stopwatch;
	// Wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();
	// Start the watch
	Watch->Start();
	// Start the loop
	while (!getShutdownFlag()) {
		Console::WriteLine("Crash Avoidance thread is running.");

		processHeartbeats();
		if (checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("Crash Avoidance thread is terminating.");
}

// Member functions
error_state CrashAvoidance::setupSharedMemory() {
	return SUCCESS;
}

error_state CrashAvoidance::processHeartbeats() {

	// Is the crash avoidance bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_CRASHAVOIDANCE) == 0) {
		// Put the crash avoidance bit up
		SM_TM_->heartbeat |= bit_CRASHAVOIDANCE;
		// Restart timer
		Watch->Restart();
	}
	else {
		// Has the time elapsed exceeded the crash time limit?
		if (Watch->ElapsedMilliseconds > CRASH_LIMIT) {
			// Shutdown all threads
			shutdownThreads();
			return ERR_TMT_FAILURE;
		}
	}

	return SUCCESS;
}

void CrashAvoidance::shutdownThreads() {
	SM_TM_->shutdown = bit_CRASHAVOIDANCE;
}

error_state CrashAvoidance::checkData() {
	return SUCCESS;
}