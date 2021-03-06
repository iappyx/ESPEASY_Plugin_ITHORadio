//#######################################################################################################
//############################## Plugin 145: Itho ventilation unit 868Mhz remote ########################
//#######################################################################################################

// author  :jodur, 	13-1-2018
// changed :jeroen, 2-11-2019
// changed :svollebregt, 30-1-2020 - changes to improve stability: volatile decleration of state,
//          disable logging within interrupts unles enabled, removed some unused code,
//          reduce noInterrupts() blockage of code fragments to prevent crashes
//			svollebregt, 16-2-2020 - ISR now sets flag which is checked by 50 per seconds plugin call as
//			receive ISR with Ticker was the cause of instability. Inspired by: https://github.com/arnemauer/Ducobox-ESPEasy-Plugin
//			svollebregt, 11-04-2020 - Minor changes to make code compatible with latest mega 20200410, removed SYNC1 option for now;
//			better to change this value in the Itho-lib code and compile it yourself
// changed :simonstar, 17-5-2020 - Major change - changed Itho-library to a modified version of the library of 'wim' https://github.com/philipsen/IthoRadio.
// This is a proof-of-concept, still crashes from time to time.

// Recommended to disable RF receive logging to minimize code execution within interrupts

// List of commands:
// 1111 to join ESP8266 with Itho ventilation unit (not implemented in this version)
// 9999 to leaveESP8266 with Itho ventilation unit (not implemented in this version)
// 0 to set Itho ventilation unit to standby (not implemented in this version)
// 1 - set Itho ventilation unit to low speed
// 2 - set Itho ventilation unit to medium speed (auto1)
// 3 - set Itho ventilation unit to high speed
// 4 - set Itho ventilation unit to full speed (not implemented in this version, switch to state 3 for compatibility)
// 13 - set itho to high speed with hardware timer (10 min)
// 23 - set itho to high speed with hardware timer (20 min)
// 33 - set itho to high speed with hardware timer (30 min)

//List of States:

// 1 - Itho ventilation unit to lowest speed
// 2 - Itho ventilation unit to medium speed (auto1)
// 3 - Itho ventilation unit to high speed
// 4 - Itho ventilation unit to full speed (not implemented in this version, switch to state 3 for compatibility)
// 13 -Itho to high speed with hardware timer (10 min)
// 23 -Itho to high speed with hardware timer (20 min)
// 33 -Itho to high speed with hardware timer (30 min)

// Usage for http (not case sensitive):
// http://ip/control?cmd=STATE,1111
// http://ip/control?cmd=STATE,1
// http://ip/control?cmd=STATE,2
// http://ip/control?cmd=STATE,3

// usage for example mosquito MQTT
// mosquitto_pub -t /Fan/cmd -m 'state 1111'
// mosquitto_pub -t /Fan/cmd -m 'state 1'
// mosquitto_pub -t /Fan/cmd -m 'state 2'
// mosquitto_pub -t /Fan/cmd -m 'state 3'

// This code needs a modified library (included) based on the version made by 'wim': https://github.com/philipsen/IthoRadio,
// originally inspired by the library made by 'supersjimmie': https://github.com/supersjimmie/IthoEcoFanRFT/tree/master/Master/Itho
// The library of 'wim' was modified by simonstar to work with ESP EASY
// A CC1101 868Mhz transmitter is needed
// See https://gathering.tweakers.net/forum/list_messages/1690945 for more information
// code/idea was inspired by first release of code from 'Thinkpad'

#include <SPI.h>
#include "IthoReceive.h"
#include "IthoSender.h"
#include "_Plugin_Helper.h"

//This extra settings struct is needed because the default settingsstruct doesn't support strings
struct PLUGIN_145_ExtraSettingsStruct
{
	char ID1[24];
	char ID2[24];
	char ID3[24];
} PLUGIN_145_ExtraSettings;

static IthoReceiveClass *PLUGIN_145_IthoReceiver = NULL;
static IthoSenderClass *PLUGIN_145_IthoSender = NULL;

class CPLUGIN_145_Data
{
public:
	int State;
	int Oldstate;
	int Timer;
	int LastIDindex;
	int OldLastIDindex;

	int8_t IRQ_pin;
	bool InitRunned;
	bool Log;
	bool Loop;

	void PLUGIN_145_DataHolder()
	{
		State = 1; // after startup it is assumed that the fan is running low
		Oldstate = 1;
		Timer = 0;
		LastIDindex = 0;
		OldLastIDindex = 0;

		IRQ_pin = -1;
		InitRunned = false;
		Log = false;
		Loop = false;
	}
};

static CPLUGIN_145_Data *PLUGIN_145_Data = NULL;

#define PLUGIN_145
#define PLUGIN_ID_145 145
#define PLUGIN_NAME_145 "Itho ventilation remote"
#define PLUGIN_VALUENAME1_145 "State"
#define PLUGIN_VALUENAME2_145 "Timer"
#define PLUGIN_VALUENAME3_145 "LastIDindex"
//#define PLUGIN_145_DEBUG true

// Timer values for hardware timer in Fan
#define PLUGIN_145_Time1 10 * 60
#define PLUGIN_145_Time2 20 * 60
#define PLUGIN_145_Time3 30 * 60


boolean Plugin_145(byte function, struct EventStruct *event, String &string)
{
	boolean success = false;

	switch (function)
	{

	case PLUGIN_DEVICE_ADD:
	{
		Device[++deviceCount].Number = PLUGIN_ID_145;
		Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
		Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
		Device[deviceCount].Ports = 0;
		Device[deviceCount].PullUpOption = false;
		Device[deviceCount].InverseLogicOption = false;
		Device[deviceCount].FormulaOption = false;
		Device[deviceCount].ValueCount = 3;
		Device[deviceCount].SendDataOption = true;
		Device[deviceCount].TimerOption = true;
		Device[deviceCount].TimerOptional = true;
		Device[deviceCount].GlobalSyncOption = true;
        Device[deviceCount].DecimalsOnly = true;
		break;
	}

	case PLUGIN_GET_DEVICENAME:
	{
		string = F(PLUGIN_NAME_145);
		break;
	}

	case PLUGIN_GET_DEVICEVALUENAMES:
	{
		strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_145));
		strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_145));
		strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_145));
		break;
	}

	case PLUGIN_INIT:
	{

		if (PLUGIN_145_Data==NULL) Serial.println("PLUGIN_INIT TEST");

		if (!PLUGIN_145_Data) {
			PLUGIN_145_Data = new CPLUGIN_145_Data();
		}

		if (!PLUGIN_145_IthoReceiver)
			PLUGIN_145_IthoReceiver = new IthoReceiveClass();

		if (!PLUGIN_145_IthoSender)
			PLUGIN_145_IthoSender = new IthoSenderClass();

		//If configured interrupt pin differs from configured, release old pin first
		if ((Settings.TaskDevicePin1[event->TaskIndex] != PLUGIN_145_Data->IRQ_pin) && (PLUGIN_145_Data->IRQ_pin != -1))
		{
			if (PLUGIN_145_Data->Log)
			{
				addLog(LOG_LEVEL_INFO, F("IO-PIN changed, detach interrupt old pin"));
			}

			detachInterrupt(PLUGIN_145_Data->IRQ_pin);
		}
		LoadCustomTaskSettings(event->TaskIndex, (byte *)&PLUGIN_145_ExtraSettings, sizeof(PLUGIN_145_ExtraSettings));

		addLog(LOG_LEVEL_INFO, F("Extra Settings PLUGIN_145 loaded"));

		PLUGIN_145_Data->IRQ_pin = Settings.TaskDevicePin1[event->TaskIndex];

		PLUGIN_145_IthoReceiver->printAllPacket = true;
		PLUGIN_145_IthoReceiver->printNonRemote = true;

		PLUGIN_145_IthoReceiver->setInterruptPin(PLUGIN_145_Data->IRQ_pin);
		PLUGIN_145_IthoReceiver->logger(PLUGIN_145_logger);
		PLUGIN_145_IthoSender->logger(PLUGIN_145_logger);
		PLUGIN_145_IthoReceiver->setup();

		PLUGIN_145_Data->InitRunned = true;

		success = true;
		break;
	}

	case PLUGIN_EXIT:
	{
		addLog(LOG_LEVEL_INFO, F("EXIT PLUGIN_145"));
		//remove interupt when plugin is removed
		//detachInterrupt(PLUGIN_145_Data->IRQ_pin);
		success = true;
		break;
	}

	case PLUGIN_ONCE_A_SECOND:
	{

		if (PLUGIN_145_Data==NULL) Serial.println("ONCE A SEC TEST");

		//decrement timer when timermode is running
		if (PLUGIN_145_Data->State >= 10)
			PLUGIN_145_Data->Timer--;

		//if timer  has elapsed set Fan state to low
		if ((PLUGIN_145_Data->State >= 10) && (PLUGIN_145_Data->Timer <= 0))
		{
			PLUGIN_145_Data->State = 1;
			PLUGIN_145_Data->Timer = 0;
		}

		//Publish new data when vars are changed or init has runned or timer is running (update every 2 sec)
		if (

			(PLUGIN_145_Data->Oldstate != PLUGIN_145_Data->State) || ((PLUGIN_145_Data->Timer > 0) && (PLUGIN_145_Data->Timer % 2 == 0)) || (PLUGIN_145_Data->OldLastIDindex != PLUGIN_145_Data->LastIDindex) || PLUGIN_145_Data->InitRunned)
		{
			if (PLUGIN_145_Data->Log)
			{
				addLog(LOG_LEVEL_INFO, F("UPDATE by PLUGIN_ONCE_A_SECOND"));
			}

			PLUGIN_145_Publishdata(event);

			sendData(event);
			//reset flag set by init
			PLUGIN_145_Data->InitRunned = false;
		}

		//Remember current state for next cycle
		PLUGIN_145_Data->Oldstate = PLUGIN_145_Data->State;
		PLUGIN_145_Data->OldLastIDindex = PLUGIN_145_Data->LastIDindex;

		success = true;
		break;
	}

	case PLUGIN_TEN_PER_SECOND:
	{

		//if (!PLUGIN_145_Data)
		//	break;

		if (PLUGIN_145_Data->Loop)
		{
			PLUGIN_145_IthoReceiver->loop();
			//yield();
			delay(0);
		}
		success = true;
		break;
	}

	case PLUGIN_READ:
	{
		// This ensures that even when Values are not changing, data is send at the configured interval for aquisition

		//if (!PLUGIN_145_Data)
		//	break;
		if (PLUGIN_145_Data==NULL) Serial.println("PLUGIN READ TEST");

		if (PLUGIN_145_Data->Log)
		{
			addLog(LOG_LEVEL_DEBUG, F("UPDATE by PLUGIN_READ"));
		}
		PLUGIN_145_Publishdata(event);
		success = true;
		break;
	}

	case PLUGIN_WRITE:
	{
		//if (!PLUGIN_145_Data)
		//	break;

		if (PLUGIN_145_Data==NULL) Serial.println("PLUGIN WRITE TEST");

		String tmpString = string;
		String cmd = parseString(tmpString, 1);
		String param1 = parseString(tmpString, 2);

		if (cmd.equalsIgnoreCase(F("STATE")))
		{

			if (param1.equalsIgnoreCase(F("1111")))
			{

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Commands 'standby', 'leave' and 'join' are not implemented in this build."));
				}
				printWebString += F("Commands 'standby', 'leave' and 'join' are not implemented in this build.");

				success = true;
			}
			if (param1.equalsIgnoreCase(F("9999")))
			{

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Commands 'standby', 'leave' and 'join' are not implemented in this build."));
				}
				printWebString += F("Commands 'standby', 'leave' and 'join' are not implemented in this build");

				success = true;
			}
			if (param1.equalsIgnoreCase(F("0")))
			{

				PLUGIN_145_Data->State = 0;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Commands 'standby', 'leave' and 'join' are not implemented in this build."));
				}
				printWebString += F("Commands 'standby', 'leave' and 'join' are not implemented in this build.");

				success = true;
			}
			if (param1.equalsIgnoreCase(F("1")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("low");

				PLUGIN_145_Data->State = 1;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'low speed' to Itho unit"));
				}

				printWebString += F("Sent command for 'low speed' to Itho unit");
				success = true;
			}
			if (param1.equalsIgnoreCase(F("2")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("auto1");

				PLUGIN_145_Data->State = 2;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'auto 1 speed' to Itho unit"));
				}

				printWebString += F("Sent command for 'auto 1 speed' to Itho unit");
				success = true;
			}
			if (param1.equalsIgnoreCase(F("3")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("high");

				PLUGIN_145_Data->State = 3;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'high speed' to Itho unit"));
				}

				printWebString += F("Sent command for 'high speed' to Itho unit");

				success = true;
			}
			if (param1.equalsIgnoreCase(F("4")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("high");

				PLUGIN_145_Data->State = 4;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'full speed' to Itho unit"));
				}

				printWebString += F("Sent command for 'full speed' to Itho unit");
				success = true;
			}
			if (param1.equalsIgnoreCase(F("13")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("timer1");

				PLUGIN_145_Data->State = 13;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time1;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'timer 1' to Itho unit"));
				}
				printWebString += F("Sent command for 'timer 1' to Itho unit");
				success = true;
			}
			if (param1.equalsIgnoreCase(F("23")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("timer2");

				PLUGIN_145_Data->State = 23;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time2;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'timer 2' to Itho unit"));
				}
				printWebString += F("Sent command for 'timer 2' to Itho unit");
				success = true;
			}
			if (param1.equalsIgnoreCase(F("33")))
			{
				PLUGIN_145_IthoSender->sendCommandRoom("timer3");

				PLUGIN_145_Data->State = 33;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time3;
				PLUGIN_145_Data->LastIDindex = 0;

				if (PLUGIN_145_Data->Log)
				{
					addLog(LOG_LEVEL_INFO, F("Sent command for 'timer 3' to Itho unit"));
				}
				printWebString += F("Sent command for 'timer 3' to Itho unit");
				success = true;
			}
		}
		break;
	}

	case PLUGIN_WEBFORM_LOAD:
	{
		addFormSubHeader(F("Remote RF Controls"));
		addFormTextBox(F("Unit #1 ID remote #1"), F("PLUGIN_145_ID1"), PLUGIN_145_ExtraSettings.ID1, 23);
		addFormNote(F("Remote #1 will be used to control the primary unit"));
		addFormTextBox(F("Unit #1 ID remote #2"), F("PLUGIN_145_ID2"), PLUGIN_145_ExtraSettings.ID2, 23);
		addFormTextBox(F("Unit #1 ID remote #3"), F("PLUGIN_145_ID3"), PLUGIN_145_ExtraSettings.ID3, 23);
		addFormSubHeader(F("Log handling / debug"));
		addFormCheckBox(F("Enable RF receive log"), F("p145_log"), PCONFIG(0));
		addFormCheckBox(F("Enable RF receive loop (disable in case OTA firmware update does not work)"), F("p145_loop"), PCONFIG(1));

		success = true;
		break;
	}

	case PLUGIN_WEBFORM_SAVE:
	{
		strcpy(PLUGIN_145_ExtraSettings.ID1, web_server.arg(F("PLUGIN_145_ID1")).c_str());
		strcpy(PLUGIN_145_ExtraSettings.ID2, web_server.arg(F("PLUGIN_145_ID2")).c_str());
		strcpy(PLUGIN_145_ExtraSettings.ID3, web_server.arg(F("PLUGIN_145_ID3")).c_str());

		SaveCustomTaskSettings(event->TaskIndex, (byte *)&PLUGIN_145_ExtraSettings, sizeof(PLUGIN_145_ExtraSettings));

		PCONFIG(0) = isFormItemChecked(F("p145_log"));
		PLUGIN_145_Data->Log = PCONFIG(0);

		PCONFIG(1) = isFormItemChecked(F("p145_loop"));
		PLUGIN_145_Data->Loop = PCONFIG(1);

		success = true;
		break;
	}
	}
	return success;
}

inline String PLUGIN_145_sub(const String &s, char separator, int cnt)
{
	int startIndex = 0;
	for (int i = 0; i < cnt; i++)
	{
		startIndex = s.indexOf(separator, startIndex);
		if (startIndex < 0)
			return "";
		startIndex++;
	}
	int lastIndex = s.indexOf(separator, startIndex);
	if (lastIndex < 0)
	{
		return s.substring(startIndex);
	}
	return s.substring(startIndex, lastIndex);
}

void PLUGIN_145_logger(const String &m)
{
	if (PLUGIN_145_Data->Log)
	{
		//Serial.print(m);
		addLog(LOG_LEVEL_INFO, String(m));
	}

	if (m.startsWith("send/remote/"))
	{

		const String &receivedRemote = PLUGIN_145_sub(m, '/', 2);
		const String &receivedCommand = PLUGIN_145_sub(m, '/', 3);

		if (PLUGIN_145_Data->Log)
		{
			addLog(LOG_LEVEL_INFO, String("Copy this command --> " + receivedCommand));
			addLog(LOG_LEVEL_INFO, String("Copy this remote  --> " + receivedRemote));
		}

		if (PLUGIN_145_RFRemoteIndex(receivedRemote) > 0)
		{

			String receivedCommandName = "";

			if (receivedCommand.equalsIgnoreCase(F("22:f1:3:63:2:4")))
			{
				receivedCommandName = F("low");
				PLUGIN_145_Data->State = 1;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f1:3:63:4:4")))
			{
				receivedCommandName = F("high");
				PLUGIN_145_Data->State = 3;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f1:3:63:3:4")))
			{
				receivedCommandName = F("auto1");
				PLUGIN_145_Data->State = 2;
				PLUGIN_145_Data->Timer = 0;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f8:3:63:2:3")))
			{
				//receivedCommandName=F("auto2");
				//not implemented
				receivedCommandName = F("unknown");
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f3:3:63:0:a")))
			{
				receivedCommandName = F("timer1");
				PLUGIN_145_Data->State = 13;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time1;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f3:3:63:0:14")))
			{
				receivedCommandName = F("timer2");
				PLUGIN_145_Data->State = 23;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time2;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else if (receivedCommand.equalsIgnoreCase(F("22:f3:3:63:0:1e")))
			{
				receivedCommandName = F("timer3");
				PLUGIN_145_Data->State = 33;
				PLUGIN_145_Data->Timer = PLUGIN_145_Time3;
				PLUGIN_145_Data->LastIDindex = PLUGIN_145_RFRemoteIndex(receivedRemote);
			}
			else
			{
				receivedCommandName = F("unknown");
			}

			if (PLUGIN_145_Data->Log)
			{
				addLog(LOG_LEVEL_INFO, String("Found remote #" + String(PLUGIN_145_RFRemoteIndex(receivedRemote)) + " " + receivedCommandName + " (" + receivedCommand + ")"));
				Serial.println(String("Found remote #" + String(PLUGIN_145_RFRemoteIndex(receivedRemote)) + " with command: " + receivedCommandName + " (" + receivedCommand + ")"));
			}
		}
	}
}

void PLUGIN_145_Publishdata(struct EventStruct *event)
{
	UserVar[event->BaseVarIndex] = PLUGIN_145_Data->State;
	UserVar[event->BaseVarIndex + 1] = PLUGIN_145_Data->Timer;
	UserVar[event->BaseVarIndex + 2] = PLUGIN_145_Data->LastIDindex;

	String log = F("State: ");
	log += UserVar[event->BaseVarIndex];
	addLog(LOG_LEVEL_DEBUG, log);
	log = F("Timer: ");
	log += UserVar[event->BaseVarIndex + 1];
	addLog(LOG_LEVEL_DEBUG, log);
	log = F("LastIDindex: ");
	log += UserVar[event->BaseVarIndex + 2];
	addLog(LOG_LEVEL_DEBUG, log);
}

int PLUGIN_145_RFRemoteIndex(String rfremoteid)
{
	if (rfremoteid == PLUGIN_145_ExtraSettings.ID1)
	{
		return 1;
	}
	else if (rfremoteid == PLUGIN_145_ExtraSettings.ID2)
	{
		return 2;
	}
	else if (rfremoteid == PLUGIN_145_ExtraSettings.ID3)
	{
		return 3;
	}
	else
	{
		return -1;
	}
}