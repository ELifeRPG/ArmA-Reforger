//------------------------------------------------------------------------------------------------
class ELIFE_ApiConfigDto : JsonApiStruct
{
	string serverUrl;

	void ELIFE_ApiConfigDto()
	{
		RegV("serverUrl");
	}
}
