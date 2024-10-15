#include "pch.h"
#include "TrainingPerformanceGraphs.h"
#include <fstream>
#include <ctime>

using namespace std;

BAKKESMOD_PLUGIN(TrainingPerformanceGraphs, "TrainingPerformanceGraphs", plugin_version, PLUGINTYPE_CUSTOM_TRAINING)

shared_ptr<CVarManagerWrapper> _globalCvarManager;

void TrainingPerformanceGraphs::onLoad()
{
	//initialize
	_globalCvarManager = cvarManager;
	cvarManager->registerCvar("tpg_enabled", "1", "Enabled TrainingPerformanceGraphs", true, false, 0, false, 0, true).bindTo(enabled);

	LOG("TPG Plugin loaded!");

	if (*enabled)
	{
		//on begin
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function GameEvent_TrainingEditor_TA.WaitingToPlayTest.OnTrainingModeLoaded", [this](ActorWrapper cw, void* params, string eventName) {
			if (!*enabled) return;

			//save TrainingPackCode
			tpCode = GetTrainingPackCode((TrainingEditorWrapper)cw.memory_address);
			if (tpCode.empty()) return;

			//save total shots in a training pack
			auto serverWrapper = gameWrapper->GetGameEventAsServer();
			if (serverWrapper.IsNull()) return;
			auto trainingWrapper = TrainingEditorWrapper(serverWrapper.memory_address);
			totalPackShots = trainingWrapper.GetTotalRounds();
			LOG("totalPackShots: " + to_string(totalPackShots));

			//save start date and time
			sessionStartTime = GetCurrentDateTime();

			LOG("Plugin enabled and pack: " + tpCode + " detected!!\nTraining session started!!");
		});

		//track shot attempts
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt", [this](ActorWrapper cw, void* params, std::string eventName) {
			shotAttempts++;
			LOG("Shot attempted! Total attempts: " + to_string(shotAttempts));
		});

		//todo: successful shots/total shots
		//todo: per shot, not per pack metrics
		//todo: assert that resets equal failed attempts (differentiate in data?)
		//todo: track boost usage
		//todo: average goal speed

		//on end
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.Destroyed", [this](ActorWrapper cw, void* params, string eventName) {
			//save end date time 
			sessionEndTime = GetCurrentDateTime();

			//save all data to csv
			SaveSessionDataToCSV(); 

			//cleanup
			if (loaded) onUnload();
			shotAttempts = 0;
			totalPackShots = -1;

			LOG("Training session ended!!");
		});
	}
}

string TrainingPerformanceGraphs::GetTrainingPackCode(TrainingEditorWrapper tw)
{
	TrainingEditorSaveDataWrapper td = tw.GetTrainingData().GetTrainingData();
	if (!td.GetCode().IsNull())
	{
		string code = td.GetCode().ToString();
		return code;
	}
	return "";
}

void TrainingPerformanceGraphs::SaveSessionDataToCSV()
{
	//gets current folder and (new or not) path 
	string dataFolder = gameWrapper->GetDataFolder().string();

	string filePath = dataFolder + "/training_sessions.csv"; //on windows this is C:\Users\<USERNAME>\AppData\Roaming\bakkesmod\bakkesmod\data

	ofstream myfile(filePath, ios::app); //will create if doesnt exist

	if (myfile.is_open())
	{
		//move to eof, if eof is 0, we haven't written anything and we need to save our header
		myfile.seekp(0, ios::end);
		if (myfile.tellp() == 0) {
			myfile << "Training Pack Code,Start Time,End Time,Shot Attempts,Total Pack Shots\n"; //creates row 0 (header)
		}
		
		if (!sessionStartTime.empty() && !sessionEndTime.empty())
		{
			//write training session to csv
			myfile
				<< tpCode << ","
				<< sessionStartTime << ","
				<< sessionEndTime << ","
				<< shotAttempts << ","
				<< totalPackShots << "\n";
			LOG("Training session data saved to CSV file");
		}
		else
		{
			LOG("Session start or end time is empty");
		}
		
		myfile.close();
	}
	else
	{
		LOG("Unable to open file");
	}
}

string TrainingPerformanceGraphs::GetCurrentDateTime()
{
	//gets current time
	time_t now = time(nullptr);

	//convert to localtime
	tm* timeInfo = localtime(&now);
	if (timeInfo == nullptr)
	{
		LOG("Failed to obtain local time");
		return "";
	}

	//format the time as a string example (1/13/2000 2:30:00)
	char buffer[20];
	if (strftime(buffer, sizeof(buffer), "%m/%d/%Y %H:%M:%S", timeInfo) == 0)
	{
		LOG("strftime failed");
		return "";
	}

	return string(buffer);
}