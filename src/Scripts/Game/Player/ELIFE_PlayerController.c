modded class SCR_PlayerController : PlayerController
{
	bool m_bPlayerCharactersFound = false;
	ref array<ref CharacterDto>> m_aPlayerCharacters = {};
	
	string m_sSelectedCharacterId = "";
	
	//------------------------------------------------------------------------------------------------
	void UpdatePlayerCharacters(array<ref CharacterDto> characters)
	{
		array<string> ids = {};
		array<string> firstNames = {};
		array<string> lastNames = {};
		
		foreach (CharacterDto character : characters)
		{
			ids.Insert(character.id);
			firstNames.Insert(character.firstName);
			lastNames.Insert(character.lastName);
		}
		
		Rpc(RpcDo_Owner_UpdatePlayerCharacters, ids, firstNames, lastNames);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_UpdatePlayerCharacters(array<string> ids, array<string> firstNames, array<string> lastNames)
	{
		for (int i = 0; i < ids.Count(); i++)
		{
			CharacterDto character = new CharacterDto();
			
			character.id = ids[i];
			character.firstName = firstNames[i];
			character.lastName = lastNames[i];
			
			m_aPlayerCharacters.Insert(character);
		}
		
		m_bPlayerCharactersFound = true;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCurrentCharacter(string characterId)
	{
		int playerId = GetPlayerId();
		Rpc(RpcAsk_Authority_SetCurrentCharacter, playerId, characterId);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SetCurrentCharacter(int playerId, string characterId)
	{
		ELIFE_PersistenceManager.GetInstance().UpdatePlayerCharacter(playerId, characterId);
	}
};