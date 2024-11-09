#include "pch.h"
#include "TrainingPerformanceGraphs.h"

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
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function GameEvent_TrainingEditor_TA.WaitingToPlayTest.OnTrainingModeLoaded", 
			[this](ActorWrapper cw, void* params, string eventName) {
			if (!*enabled) return;

			//get and store name, code, total pack shots
			GetTrainingPackInfo((TrainingEditorWrapper)cw.memory_address);
			if (info.name.empty() || info.code.empty() || info.totalPackShots < 0) return;

			//get session start time
			sessionStartTime = GetCurrentDateTime();

			LOG("Training Session Started Successfully!"); 
			LOG("Training Pack Name : " + info.name + " Training Pack Code : " + info.code + " Total Pack Shots : " + to_string(info.totalPackShots));
		});

		//on round (shot) change, update the active round index
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.EventRoundChanged",
			[this](ActorWrapper cw, void* params, string eventName) {
			roundNumber = TrainingEditorWrapper(cw.memory_address).GetActiveRoundNumber();
			LOG("Round: " + to_string(roundNumber + 1) + " / " + to_string(info.totalPackShots));
		});

		//on start and end on shot attempt
		gameWrapper->HookEvent("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt",
			[this](string) { HandleAttempt(true); });
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.EndRound",
			[this](string) { HandleAttempt(false); }); 

		//on begin and end state of boost usage, accurately records boost usage
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.BeginState",
			[this](string) { HandleBoost(true); });
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.EndState",
			[this](string) { HandleBoost(false); });

		gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal",
			[this](string) { GetSpeed(isAttempting); });

		//on success increment shotsMade, and get goal speed
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.GoalScored",
			[this](string) { shotsMade++; LOG("Shots made: " + to_string(shotsMade)); });
		
		//todo: per shot, not per pack metrics
		// round implemented, continue
		
		//on end
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.Destroyed",
			[this](ActorWrapper cw, void* params, string eventName) {
			
			sessionEndTime = GetCurrentDateTime(); //save end date time 
			
			SaveSessionDataToCSV(); //write to CSV

			//cleanup
			if (loaded) onUnload();
			shotAttempts = 0;
			shotsMade = 0;
			info.totalPackShots = 0;
			info.name.clear();
			info.code.clear();

			LOG("Training session ended!");
		});
	}
}

TrainingPackInfo TrainingPerformanceGraphs::GetTrainingPackInfo(TrainingEditorWrapper tw) {
	TrainingEditorSaveDataWrapper td = tw.GetTrainingData().GetTrainingData();
	info.code = td.GetCode().ToString();
	info.name = td.GetTM_Name().ToString();
	info.totalPackShots = tw.GetTotalRounds();

	return info;
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
			myfile << "Start Time,End Time,Training Pack Code,Training Pack Name,Total Pack Shots,Boost Used,Goal Speed,Shots Made,Shot Attempts\n"; //creates row 0 (header)
		}
		
		if (!sessionStartTime.empty() && !sessionEndTime.empty())
		{
			//write training session to csv
			myfile
				<< sessionStartTime << ","
				<< sessionEndTime << ","
				<< info.code << ","
				<< info.name << ","
				<< to_string(info.totalPackShots) << ","
				<< totalBoostUsed << ","
				<< speed << ","
				<< shotsMade << ","
				<< shotAttempts << "\n";
				
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

void TrainingPerformanceGraphs::HandleAttempt(bool attempting) {
	if (attempting) {
		shotAttempts++; 
		totalBoostUsed = 0.0f;
		isAttempting = true;

		LOG("Attempt " + to_string(shotAttempts) + " started, boost tracking enabled.");
	}
	else {
		isAttempting = false;
		isBoosting = false;
		LOG("Total boost used in attempt " + to_string(totalBoostUsed));
	}
}

void TrainingPerformanceGraphs::HandleBoost(bool start) {
	if (isAttempting) {
		if (start && !isBoosting) {
			isBoosting = true;
			boostStartTime = chrono::high_resolution_clock::now();
		}
		else if (!start && isBoosting) {
			auto boostEndTime = chrono::high_resolution_clock::now();
			auto duration = chrono::duration_cast<chrono::milliseconds>(boostEndTime - boostStartTime).count();

			float boostConsumptionRate = 33.3f;
			float boostUsed = (duration / 1000.0f) * boostConsumptionRate;
			totalBoostUsed += boostUsed;

			isBoosting = false;
			//LOG("Boost used: " + to_string(boostUsed) + ", Total boost used: " + to_string(totalBoostUsed));
		}
	}
}

void TrainingPerformanceGraphs::GetSpeed(bool attempting)
{
	if (attempting) {
		ServerWrapper server = gameWrapper->GetGameEventAsServer();
		if (server.IsNull()) return;

		BallWrapper ball = server.GetBall();
		if (ball.IsNull()) return;

		speed = ball.GetVelocity().magnitude() * 0.036f; //convert unreal unit per second to kph (0.036)
		LOG("Goal Speed: " + to_string(speed));
	}
}