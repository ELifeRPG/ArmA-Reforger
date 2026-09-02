//------------------------------------------------------------------------------------------------
class ELIFE_PhoneDto : JsonApiStruct
{
	string id;
	string number;
	string registeredTo;
	string status;
	bool isPoweredOn;
	ref array<string> blockedNumbers = {};
	ref array<string> installedApps = {};

	void ELIFE_PhoneDto()
	{
		RegV("id");
		RegV("number");
		RegV("registeredTo");
		RegV("status");
		RegV("isPoweredOn");
		RegV("blockedNumbers");
		RegV("installedApps");
	}
}
