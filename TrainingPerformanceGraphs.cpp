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

			//refit our global vector shotData 
			shotData.resize(info.totalPackShots);

			//get session start time
			sessionStartTime = GetCurrentDateTime();

			LOG("Training Session Started Successfully!"); 
			LOG("Training Pack Name : " + info.name + " Training Pack Code : " + info.code + " Total Pack Shots : " + to_string(info.totalPackShots));
		});

		//on round (shot) change, update the active round index
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.EventRoundChanged",
			[this](ActorWrapper cw, void* params, string eventName) {
			roundIndex = TrainingEditorWrapper(cw.memory_address).GetActiveRoundNumber();
			LOG("Round: " + to_string(roundIndex + 1) + " / " + to_string(info.totalPackShots));
		});

		//on start and end on shot attempt
		gameWrapper->HookEvent("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt",
			[this](string) { HandleAttempt(true); });
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.EndRound",
			[this](string) { 
				if (roundIndex < 0 || roundIndex >= shotData.size() || shotData[roundIndex].attempts.empty()) {
					LOG("Error: roundIndex or attempt data out of bounds for GoalScored");
					return;
				}

				//finalize boost used for this attempt
				//this MUST be done here for safety, doing at GoalScored will not work
				HandleBoost(false);
				shotData[roundIndex].attempts.back().boostUsed = totalBoostUsed; 

				LOG("Data set for shot " + to_string(roundIndex + 1)
					+ ", Boost used = " + to_string(totalBoostUsed));

				HandleAttempt(false); 
			});

		//on begin and end state of boost usage, accurately records boost usage
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.BeginState",
			[this](string) { HandleBoost(true); });
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.EndState",
			[this](string) { HandleBoost(false); });

		gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal",
			[this](string) { GetSpeed(isAttempting);});

		//on success increment shotsMade, and get goal speed
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.GoalScored",
			[this](string){
				if (roundIndex < 0 || roundIndex >= shotData.size() || shotData[roundIndex].attempts.empty()) {
					LOG("Error: roundIndex or attempt data out of bounds for GoalScored");
					return;
				}
				
				shotData[roundIndex].attempts.back().scored = true;
				shotData[roundIndex].attempts.back().goalSpeed = speed;

				LOG("Data set for shot " + to_string(roundIndex + 1)
					+ ": Scored = " + to_string(true)
					+ ", Speed = " + to_string(speed));
			});
		
		//on end
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.EndTraining",
			[this](ActorWrapper cw, void* params, string eventName) {
			
			sessionEndTime = GetCurrentDateTime(); //save end date time 
			
			SaveSessionDataToJSON(); //write to JSON

			//cleanup
			if (loaded) onUnload();
			shotData.clear();
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
		shotData[roundIndex].attempts.emplace_back();  //add default AttemptData

		totalBoostUsed = 0.0f;
		isAttempting = true;

		LOG("Attempt " + to_string(shotData[roundIndex].attempts.size()) + " started for shot " + to_string(roundIndex + 1));
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
			//LOG("TRACKING");
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
			//LOG("NOT TRACKING");
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

void TrainingPerformanceGraphs::SaveSessionDataToJSON()
{
	//set up path
	string dataFolder = gameWrapper->GetDataFolder().string();
	string filePath = dataFolder + "/training_sessions.json"; //on windows this is C:\Users\<USERNAME>\AppData\Roaming\bakkesmod\bakkesmod\data

	//initialize
	ostringstream jsonStream;
	bool isFirstSession = true;
	string existingData;

	ifstream infile(filePath);
	if (infile.good()) {
		stringstream buffer;
		buffer << infile.rdbuf();
		existingData = buffer.str();
		infile.close();

		//if existing data has the closing brackets, and remove them for appending
		if (!existingData.empty() && existingData.find("]") != string::npos) {
			existingData.erase(existingData.find_last_of("]"), string::npos); // Remove the closing brackets
			isFirstSession = false;
			jsonStream << existingData << ",\n";
		}
	}

	//if new file write beginning
	if (isFirstSession) {
		jsonStream << "{\n    \"sessions\": [\n";
	}

	//append new session data
	jsonStream << "        {\n";
	jsonStream << "            \"startTime\": \"" << sessionStartTime << "\",\n";
	jsonStream << "            \"endTime\": \"" << sessionEndTime << "\",\n";
	jsonStream << "            \"trainingPack\": {\n";
	jsonStream << "                \"code\": \"" << info.code << "\",\n";
	jsonStream << "                \"name\": \"" << info.name << "\",\n";
	jsonStream << "                \"totalPackShots\": " << info.totalPackShots << "\n";
	jsonStream << "            },\n";
	jsonStream << "            \"shotData\": {\n";

	//fun
	for (size_t shotIndex = 0; shotIndex < shotData.size(); ++shotIndex) {
		jsonStream << "                \"shot " << shotIndex << "\": {\n";
		jsonStream << "                    \"attempts\": {\n";

		const auto& attempts = shotData[shotIndex].attempts;
		for (size_t attemptIndex = 0; attemptIndex < attempts.size(); ++attemptIndex) {
			const AttemptData& attempt = attempts[attemptIndex];
			jsonStream << "                        \"attempt " << attemptIndex << "\": { ";
			jsonStream << "\"scored\": " << (attempt.scored ? "true" : "false") << ", ";
			jsonStream << "\"goalSpeed\": " << (attempt.scored ? attempt.goalSpeed : 0.0) << ", ";
			jsonStream << "\"boostUsed\": " << attempt.boostUsed;
			jsonStream << " }";
			if (attemptIndex < attempts.size() - 1) {
				jsonStream << ",";
			}
			jsonStream << "\n";
		}

		jsonStream << "                    }\n";
		jsonStream << "                }";
		if (shotIndex < shotData.size() - 1) {
			jsonStream << ",";
		}
		jsonStream << "\n";
	}

	//closings
	jsonStream << "            }\n";
	jsonStream << "        }";
	jsonStream << "\n    ]\n}\n";

	//write final json to file
	ofstream myfile(filePath, ios::out | ios::trunc); //overwrite the file
	if (myfile.is_open()) {
		myfile << jsonStream.str();
		myfile.close();
		LOG("Training session data appended to JSON file");
	}
	else {
		LOG("Unable to open file");
	}
}