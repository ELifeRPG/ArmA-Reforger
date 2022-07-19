class SessionDto : JsonApiStruct
{
    int steamId;
    string accountId;
	
	void SessionDto()
	{
		RegV("steamId");
		RegV("accountId");
	}
}
