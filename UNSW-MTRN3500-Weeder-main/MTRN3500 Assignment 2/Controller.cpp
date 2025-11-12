#include "Controller.h"
#pragma managed(push, off)
#include "ControllerInterface.h"
#pragma managed(pop)

// Constructors
Controller::Controller(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl) {
	SM_TM_ = SM_TM;
	SM_VehicleControl_ = SM_VehicleControl;
	Watch = gcnew Stopwatch;
	xBox_ = new ControllerInterface(1, 1);
}

Controller::~Controller() {
	delete xBox_;
}

// Derived from UGVModule
error_state Controller::processSharedMemory() {

	// Request Vehicle Control Mutex
	Monitor::Enter(SM_VehicleControl_->lockObject);

	if (!xBox_->GetState().isConnected) {
		SM_VehicleControl_->Speed = 0;
		SM_VehicleControl_->Steering = 0;
		printError(ERR_CONTROLLER_FAILURE);
	}
	else {
		SM_VehicleControl_->Speed = xBox_->GetState().rightTrigger - xBox_->GetState().leftTrigger;
		//SM_VehicleControl_->Speed = xBox_->GetState().leftThumbY;
		SM_VehicleControl_->Steering -= xBox_->GetState().rightThumbX;
		if (SM_VehicleControl_->Steering != 0 && xBox_->GetState().rightThumbX == 0) {
			if (SM_VehicleControl_->Steering > 0) SM_VehicleControl_->Steering--;
			if (SM_VehicleControl_->Steering < 0) SM_VehicleControl_->Steering++;
		}
		// Cap at maximum speed and steering angle
		if (SM_VehicleControl_->Speed > MAX_SPEED) SM_VehicleControl_->Speed = MAX_SPEED;
		if (SM_VehicleControl_->Speed < -MAX_SPEED) SM_VehicleControl_->Speed = -MAX_SPEED;
		if (SM_VehicleControl_->Steering > MAX_STEERING) SM_VehicleControl_->Steering = MAX_STEERING;
		if (SM_VehicleControl_->Steering < -MAX_STEERING) SM_VehicleControl_->Steering = -MAX_STEERING;
	}

	// Release Vehicle Control Mutex
	Monitor::Exit(SM_VehicleControl_->lockObject);

	return SUCCESS;
}

bool Controller::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_CONTROLLER);
}

void Controller::threadFunction() {
	Console::WriteLine("Controller thread is starting.");
	// Setup the stopwatch
	Watch = gcnew Stopwatch;
	// Wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();
	// Start the watch
	Watch->Start();
	// Start the loop
	while (!getShutdownFlag()) {
		Console::WriteLine("Controller thread is running.");

		// If button A is pressed, shutdown Controller thread
		if (xBox_->GetState().buttonA) break;

		processHeartbeats();
		if (checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("Controller thread is terminating.");
}

// Member functions
error_state Controller::setupSharedMemory() {
	return SUCCESS;
}

error_state Controller::processHeartbeats() {

	// Is the controller bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_CONTROLLER) == 0) {
		// Put the controller bit up
		SM_TM_->heartbeat |= bit_CONTROLLER;
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

void Controller::shutdownThreads() {
	SM_TM_->shutdown = bit_CONTROLLER;
}

error_state Controller::checkData() {
	return SUCCESS;
}