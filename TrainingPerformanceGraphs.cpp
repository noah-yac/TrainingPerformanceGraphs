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
		//ensure correct training pack
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.LoadRound", [this](ActorWrapper cw, void* params, string eventName) {
			if (!*enabled || !IsTrainingPackSelected((TrainingEditorWrapper)cw.memory_address))
				return;
			LOG("Plugin enabled and pack detected!!\nTraining session started!!");

			sessionStartTime = GetCurrentDateTime(); //get start date and time

			//TODO: add more values
		});

		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.Destroyed", [this](ActorWrapper cw, void* params, string eventName) {
			LOG("Training session ended!!");
			
			sessionEndTime = GetCurrentDateTime(); //get end date and time 

			SaveSessionDataToCSV(); //save all data to csv
			
			if (loaded) onUnload(); //unload
		});
	}
}

bool TrainingPerformanceGraphs::IsTrainingPackSelected(TrainingEditorWrapper tw)
{
	if (!tw.IsNull())
	{
		GameEditorSaveDataWrapper data = tw.GetTrainingData();
		if (!data.IsNull())
		{
			TrainingEditorSaveDataWrapper td = data.GetTrainingData();
			if (!td.IsNull())
			{
				if (!td.GetCode().IsNull())
				{
					auto code = td.GetCode().ToString();
					if (code == "7028-5E10-88EF-E83E")
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void TrainingPerformanceGraphs::SaveSessionDataToCSV()
{
	//gets current folder and (new or not) path 
	string dataFolder = gameWrapper->GetDataFolder().string();

	string filePath = dataFolder + "/training_sessions.csv"; //on windows this is C:\Users\<USERNAME>\AppData\Roaming\bakkesmod\bakkesmod\data

	ofstream myfile(filePath, ios::app); //will create if doesnt exist

	if (myfile.is_open())
	{
		//if file is new 
		if (myfile.tellp() == 0)
		{
			myfile << "Start Time,End Time,Placeholder1,Placeholder2\n"; //creates row 0 (header)
		}
		
		if (!sessionStartTime.empty() || !sessionEndTime.empty())
		{
			//write training session to csv
			myfile << sessionStartTime << ","
				<< sessionEndTime << ","
				<< "placeholder1" << ","
				<< "placeholder2" << "\n";
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