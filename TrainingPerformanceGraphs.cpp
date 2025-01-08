#include "pch.h"
#include "TrainingPerformanceGraphs.h"

BAKKESMOD_PLUGIN(TrainingPerformanceGraphs, "TrainingPerformanceGraphs", plugin_version, PLUGINTYPE_CUSTOM_TRAINING)

shared_ptr<CVarManagerWrapper> _globalCvarManager;

void TrainingPerformanceGraphs::onLoad()
{
	//initialize
	_globalCvarManager = cvarManager;
	cvarManager->registerCvar("tpg_enabled", "1", "Enabled TrainingPerformanceGraphs", true, false, 0, false, 0, true).bindTo(enabled);

	//LOG("TPG Plugin loaded!");

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

			//LOG("Training Session Started Successfully!"); 
			//LOG("Training Pack Name : " + info.name 
			//	+ " Training Pack Code : "  + info.code 
			//	+ " Total Pack Shots : "  + to_string(info.totalPackShots)
			//);

		});

		//on round (shot) change, update the active round index
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.EventRoundChanged",
			[this](ActorWrapper cw, void* params, string eventName) {
			roundIndex = TrainingEditorWrapper(cw.memory_address).GetActiveRoundNumber();
			//LOG("Round: " + to_string(roundIndex + 1) + " / " + to_string(info.totalPackShots));
		});

		//on start and end on shot attempt
		gameWrapper->HookEvent("Function TAGame.TrainingEditorMetrics_TA.TrainingShotAttempt",
			[this](string) { HandleAttempt(true); });
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.EndRound",
			[this](string) { 
				if (roundIndex < 0 || roundIndex >= shotData.size() || shotData[roundIndex].attempts.empty()) {
					LOG("roundIndex or attempt data out of bounds for GoalScored!");
					return;
				}

				//finalize boost used for this attempt this MUST be done here for safety, doing at GoalScored doesnt work
				HandleBoost(false);
				shotData[roundIndex].attempts.back().boostUsed = totalBoostUsed; 

				//LOG("Data set for shot " + to_string(roundIndex + 1)
				//	+ ", Boost used = " + to_string(totalBoostUsed));

				HandleAttempt(false); 
			});

		//on begin and end state of boost usage, accurately records boost usage
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.BeginState",
			[this](string) { HandleBoost(true); });
		gameWrapper->HookEvent("Function CarComponent_Boost_TA.Active.EndState",
			[this](string) { HandleBoost(false); });

		
		//get goal speed and/or on success, increment shotsMade
		gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal",
			[this](string) { GetSpeed(isAttempting); });
		gameWrapper->HookEvent("Function TAGame.GameMetrics_TA.GoalScored",
			[this](string){
				if (roundIndex < 0 || roundIndex >= shotData.size() || shotData[roundIndex].attempts.empty()) {
					LOG("roundIndex or attempt data out of bounds for GoalScored!");
					return;
				}
				
				shotData[roundIndex].attempts.back().scored = true;
				shotData[roundIndex].attempts.back().goalSpeed = speed;

				//LOG("Data set for shot " + to_string(roundIndex + 1)
				//	+ ": Scored = " + to_string(true)
				//	+ ", Speed = " + to_string(speed));
			});
		
		//on end
		gameWrapper->HookEventWithCaller<ActorWrapper>("Function TAGame.GameEvent_TrainingEditor_TA.EndTraining",
			[this](ActorWrapper cw, void* params, string eventName) {
			
			sessionEndTime = GetCurrentDateTime(); //save end date time 
			
			SaveSessionDataToXML(); //write to XML

			//cleanup
			if (loaded) onUnload();
			shotData.clear();
			info.totalPackShots = 0;
			info.name.clear();
			info.code.clear();
			roundIndex = -1;

			//LOG("Training session ended!");
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
		LOG("Failed to obtain local time!");
		return "";
	}

	//format the time as a string example (1/13/2000 2:30:00)
	char buffer[20];
	if (strftime(buffer, sizeof(buffer), "%m/%d/%Y %H:%M:%S", timeInfo) == 0)
	{
		LOG("Formating time as a string failed!");
		return "";
	}

	return string(buffer);
}

void TrainingPerformanceGraphs::HandleAttempt(bool attempting) {
	if (attempting) {
		shotData[roundIndex].attempts.emplace_back();

		totalBoostUsed = 0.0f;
		isAttempting = true;

		//LOG("Attempt " + to_string(shotData[roundIndex].attempts.size()) + " started for shot " + to_string(roundIndex + 1));
	}
	else {
		isAttempting = false;
		isBoosting = false;
		//LOG("Total boost used in attempt " + to_string(totalBoostUsed));
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
		//LOG("Goal Speed: " + to_string(speed));
	}
}

void TrainingPerformanceGraphs::SaveSessionDataToXML()
{
	//set up path
	string dataFolder = gameWrapper->GetDataFolder().string();
	string filePath = dataFolder + "/training_sessions.xml"; //on windows this is C:\Users\<USERNAME>\AppData\Roaming\bakkesmod\bakkesmod\data

	//initialize
	ostringstream xmlStream;
	bool isFirstSession = true;
	string existingData;

	ifstream infile(filePath);
	if (infile.good()) {
		stringstream buffer;
		buffer << infile.rdbuf();
		existingData = buffer.str();
		infile.close();

		//check if the file already contains data, and remove the closing </sessions> tag if present
		size_t closeTagPos = existingData.find("</sessions>");
		if (closeTagPos != string::npos) {
			existingData = existingData.substr(0, closeTagPos);
			isFirstSession = false;
			xmlStream << existingData;
		}
	}

	//if new file place start file tags
	if (isFirstSession) {
		xmlStream << "<root>\n    <sessions>\n";
	}

	//append session data
	xmlStream << "        <session>\n";
	xmlStream << "            <startTime>" << sessionStartTime << "</startTime>\n";
	xmlStream << "            <endTime>" << sessionEndTime << "</endTime>\n";
	xmlStream << "            <trainingPack>\n";
	xmlStream << "                <code>" << info.code << "</code>\n";
	xmlStream << "                <name>" << info.name << "</name>\n";
	xmlStream << "                <totalPackShots>" << info.totalPackShots << "</totalPackShots>\n";
	xmlStream << "            </trainingPack>\n";
	xmlStream << "            <shotData>\n";

	//add shots and attempts
	for (size_t shotIndex = 0; shotIndex < shotData.size(); ++shotIndex) {
		xmlStream << "                <shot id=\"" << shotIndex << "\">\n";
		const auto& attempts = shotData[shotIndex].attempts;

		for (size_t attemptIndex = 0; attemptIndex < attempts.size(); ++attemptIndex) {
			const AttemptData& attempt = attempts[attemptIndex];
			xmlStream << "                    <attempt id=\"" << attemptIndex << "\" "
				<< "scored=\"" << (attempt.scored ? "true" : "false") << "\" "
				<< "goalSpeed=\"" << (attempt.scored ? attempt.goalSpeed : 0.0) << "\" "
				<< "boostUsed=\"" << attempt.boostUsed << "\" />\n";
		}

		xmlStream << "                </shot>\n";
	}

	//closings
	xmlStream << "            </shotData>\n";
	xmlStream << "        </session>\n";

	//if first session, close the root tag
	if (isFirstSession) {
		xmlStream << "    </sessions>\n</root>\n";
	}
	else {
		xmlStream << "    </sessions>\n";
	}

	//write final XML to file
	ofstream myfile(filePath, ios::out | ios::trunc); //overwrite the file
	if (myfile.is_open()) {
		myfile << xmlStream.str();
		myfile.close();
		LOG("Training session data appended to XML file!");
	}
	else {
		LOG("Unable to open file!");
	}
}