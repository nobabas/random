#include "TMM.h"

error_state ThreadManagement::processSharedMemory() {
	return SUCCESS;
}

bool ThreadManagement::getShutdownFlag() {
	return (SM_TM_->shutdown & bit_TM);
}

void ThreadManagement::threadFunction() {
	
	Console::WriteLine("TMT Thread is starting.");
	// Make a list of thread properties
	ThreadPropertiesList = gcnew array<ThreadProperties^>{
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Laser(SM_TM_, SM_Laser_), &Laser::threadFunction), true, "Laser thread", bit_LASER),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew GNSS(SM_TM_, SM_GNSS_), &GNSS::threadFunction), false, "GNSS thread", bit_GNSS),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Controller(SM_TM_, SM_VehicleControl_), &Controller::threadFunction), true, "Controller thread", bit_CONTROLLER),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew VehicleControl(SM_TM_, SM_VehicleControl_), &VehicleControl::threadFunction), true, "Vehicle Control thread", bit_VC),
		gcnew ThreadProperties(gcnew ThreadStart(gcnew Display(SM_TM_, SM_Laser_), &Display::threadFunction), true, "Display thread", bit_DISPLAY),
		// gcnew ThreadProperties(gcnew ThreadStart(gcnew CrashAvoidance(SM_TM_, SM_Laser_, SM_VehicleControl_), &CrashAvoidance::threadFunction), false, "Crash Avoidance thread", bit_CRASHAVOIDANCE),
	};
	// Make a list of threads
	ThreadList = gcnew array<Thread^>(ThreadPropertiesList->Length);
	// Make the stopwatch list
	SM_TM_->WatchList = gcnew array<Stopwatch^>(ThreadPropertiesList->Length);
	// Make thread barrier
	SM_TM_->ThreadBarrier = gcnew Barrier(ThreadPropertiesList->Length + 1); // +1 Need to include this thread
	// Start all the threads
	for (int i{ 0 }; i < ThreadPropertiesList->Length; i++) {
		SM_TM_->WatchList[i] = gcnew Stopwatch;	// Initialise StopwatchList
		ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
		ThreadList[i]->Start();
	}
	// Wait at the TMT thread barrier
	SM_TM_->ThreadBarrier->SignalAndWait();
	// Start all the stopwatches
	for (int i{ 0 }; i < ThreadList->Length; i++) {
		SM_TM_->WatchList[i]->Start();
	}
	// Start the thread loops. If shutdown flag is 1, terminate the loop
	while (!getShutdownFlag()) {
		// If 'q' is pressed, terminate the loop
		if (Console::KeyAvailable) {
			if (Console::ReadKey().Key == ConsoleKey::Q) break;
		}
		Console::WriteLine("TMT Thread is running.");
		// Keep checking the heartbeats
		processHeartbeats();
		Thread::Sleep(50);
	}
	// Shutdown threads
	shutdownModules();
	// Join all threads
	for (int i{ 0 }; i < ThreadPropertiesList->Length; i++) {
		ThreadList[i]->Join();
	}
	Console::WriteLine("TMT Thread is terminating.");
}

error_state ThreadManagement::setupSharedMemory() {

	SM_TM_ = gcnew SM_ThreadManagement;
	SM_Laser_ = gcnew SM_Laser;
	SM_GNSS_ = gcnew SM_GNSS;
	SM_VehicleControl_ = gcnew SM_VehicleControl;

	return error_state::SUCCESS;
}

void ThreadManagement::shutdownModules() {
	SM_TM_->shutdown = bit_ALL;
}

error_state ThreadManagement::processHeartbeats() {

	for (int i{ 0 }; i < ThreadList->Length; i++) {
		// Check the heartbeat flag of i th thread, is it high?
		if (SM_TM_->heartbeat & ThreadPropertiesList[i]->BitID) {
			// If high, put i th bit down
			SM_TM_->heartbeat ^= ThreadPropertiesList[i]->BitID;
			// Restart the stopwatch
			SM_TM_->WatchList[i]->Restart();
		}
		else {
			// If low, check the stopwatch, is time exceed crash time limit?
			if (SM_TM_->WatchList[i]->ElapsedMilliseconds > CRASH_LIMIT) {
				// Is i th process a critical process?
				if (ThreadPropertiesList[i]->Critical) {
					// Shutdown all
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " failure. Shutting down all threads.");
					shutdownModules();
					return ERR_CRITICAL_PROCESS_FAILURE;
				}
				else {
					Console::WriteLine(ThreadPropertiesList[i]->ThreadName + " failed. Attempting to restart.");
					ThreadList[i]->Abort();
					// else make the i th thread
					ThreadList[i] = gcnew Thread(ThreadPropertiesList[i]->ThreadStart_);
					// Change only one thread to wait, if this thread crash once, it passes the barrier immediately, instaead of failing three times
					SM_TM_->ThreadBarrier = gcnew Barrier(1);
					// Try to restart
					ThreadList[i]->Start();
				}
			}
		}
	}

	return error_state::SUCCESS;
}