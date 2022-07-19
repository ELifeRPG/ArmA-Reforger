modded class ELIFE_Api
{
	void GetAccountCharacters(string accountId, Managed instance = null, string functionName = "")
	{
		ELIFE_CharacterDtoListResultDtoCallback cbx = new ELIFE_CharacterDtoListResultDtoCallback;
		cbx.SetCallback(instance, functionName, accountId);
		GetElifeApi().GET(cbx, string.Format("accounts/%1/characters", accountId));
	}
}