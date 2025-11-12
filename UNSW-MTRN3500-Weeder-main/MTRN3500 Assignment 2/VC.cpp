#include "VC.h"

// Constructors
VehicleControl::VehicleControl(SM_ThreadManagement^ SM_TM, SM_VehicleControl^ SM_VehicleControl) {

	SM_TM_ = SM_TM;
	SM_VehicleControl_ = SM_VehicleControl;
	Watch = gcnew Stopwatch;
	SendData = gcnew array<unsigned char>(1024);
	ReadData = gcnew array<unsigned char>(1024);
	SM_VehicleControl_->StopFlag = false;
}

VehicleControl::~VehicleControl() {
	Client->Close();
}

// Derived from UGVModule
error_state VehicleControl::processSharedMemory() {

	// Request Vehicle Control Mutex
	Monitor::Enter(SM_VehicleControl_->lockObject);

	// If object is detected, stop immediately
	if (SM_VehicleControl_->StopFlag) {
		SM_VehicleControl_->Speed = 0.0;
		SM_VehicleControl_->Steering = 0.0;
	}
	String^ speed = Convert::ToString(SM_VehicleControl_->Speed);
	String^ steering = Convert::ToString(SM_VehicleControl_->Steering);

	// Release Vehicle Control Mutex
	Monitor::Exit(SM_VehicleControl_->lockObject);

	// Alternating watchdog
	Watchdog = !Watchdog;
	String^ watchdog = Convert::ToString(Convert::ToInt16(Watchdog));
	String^ command = "# " + steering + " " + speed + " " + watchdog + " #";
	SendData = Encoding::ASCII->GetBytes(command);

	if (Stream == nullptr) return ERR_VC_FAILURE;
	
	// Send start-of-text (STX) first
	Stream->WriteByte(0x02);
	// Send the command
	Stream->Write(SendData, 0, SendData->Length);
	// Send end-of-text (ETX)
	Stream->WriteByte(0x03);

	return SUCCESS;
}

bool VehicleControl::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_VC);
}

void VehicleControl::threadFunction() {

	Console::WriteLine("Vehicle Control thread is starting.");
	// Connect to vehicle control
	error_state state = connect(WEEDER_ADDRESS, 25000);
	if (state != SUCCESS) {
		printError(state);
		shutdownThreads();
	}
	// Setup the stopwatch
	Watch = gcnew Stopwatch;
	// Wait at the barrier
	SM_TM_->ThreadBarrier->SignalAndWait();
	// Start the watch
	Watch->Start();
	// Start the loop
	while (!getShutdownFlag()) {
		Console::WriteLine("Vehicle Control thread is running.");

		processHeartbeats();
		if (communicate() == SUCCESS && checkData() == SUCCESS) {
			// Do something with the data...
			processSharedMemory();
		}
		Thread::Sleep(20);
	}
	Console::WriteLine("Vehicle Control thread is terminating.");
}

// Derived from NetworkedModule
error_state VehicleControl::connect(String^ hostName, int portNumber) {

	try {
		Client = gcnew TcpClient(hostName, portNumber);
	}
	catch (...) {
		return ERR_CONNECTION;
	}

	Stream = Client->GetStream();
	if (Stream == nullptr) return ERR_VC_FAILURE;

	Client->NoDelay = true;
	Client->ReceiveTimeout = 500;
	Client->SendTimeout = 500;
	Client->ReceiveBufferSize = 1024;
	Client->SendBufferSize = 1024;

	// Authentication
	try {
		SendData = Encoding::ASCII->GetBytes(AUTHENTICATION_REQUEST);
		Stream->Write(SendData, 0, SendData->Length);
		Threading::Thread::Sleep(10);
		Stream->Read(ReadData, 0, ReadData->Length);
		String^ Response = Encoding::ASCII->GetString(ReadData);
		if (String::Compare(Response, AUTHENTICATION_RESPONSE) != 0) return ERR_AUTHENTICATION_FAILURE;
	}
	catch (...) {
		return ERR_AUTHENTICATION_FAILURE;
	}

	return SUCCESS;
}

error_state VehicleControl::communicate() {
	return SUCCESS;
}

// Member functions
error_state VehicleControl::setupSharedMemory() {
	return SUCCESS;
}

error_state VehicleControl::processHeartbeats() {

	// Is the VC bit in the heartbeat byte down?
	if ((SM_TM_->heartbeat & bit_VC) == 0) {
		// Put the VC bit up
		SM_TM_->heartbeat |= bit_VC;
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

void VehicleControl::shutdownThreads() {
	SM_TM_->shutdown = bit_VC;
}

error_state VehicleControl::checkData() {
	return SUCCESS;
}

