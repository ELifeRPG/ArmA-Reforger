//------------------------------------------------------------------------------------------------
class ELIFE_PhoneAppDto : JsonApiStruct
{
	string key;
	string displayName;

	void ELIFE_PhoneAppDto()
	{
		RegV("key");
		RegV("displayName");
	}
}
