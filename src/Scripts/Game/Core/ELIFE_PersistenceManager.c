class ELIFE_PersistenceManager
{
	protected static ref ELIFE_PersistenceManager s_PersistenceManagerInstance;
	
	protected ref map<string, array<ref CharacterDto>> m_mPlayerCharacters = new map<string, array<ref CharacterDto>>;
	
	protected ref map<string, CharacterDto> m_mSelectedCharacters = new map<string, CharacterDto>;
	
	//------------------------------------------------------------------------------------------------
	void GetCharactersForPlayer(string playerUid)
	{
		ELIFE_Api.GetInstance().GetAccountCharacters(playerUid, this, "SaveCharactersToMap");
	}
	protected void SaveCharactersToMap(ELIFE_EApiStatusCode status, CharacterDtoListResultDto characterList, string accountId)
	{
		if (status != ELIFE_EApiStatusCode.SUCCESS)
			return;
		
		if (m_mPlayerCharacters.Contains(accountId))
		{
			m_mPlayerCharacters.Remove(accountId);
		}
		
		array<ref CharacterDto> characters = {};
		
		foreach (CharacterDto character : characterList.data)
		{
			characters.Insert(character);
		}
		
		m_mPlayerCharacters.Insert(accountId, characters);
		
		// Share character data to player
		int playerRepId = ELIFE_PlayerIdentity.GetInstance().GetPlayerRepIdFromPlayerUid(accountId);
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerRepId));
		
		array<ref CharacterDto> playerCharacters = {};
		if (m_mPlayerCharacters.Contains(accountId))
			playerCharacters = m_mPlayerCharacters.Get(accountId);
		
		playerController.UpdatePlayerCharacters(playerCharacters);
	}
	
	//------------------------------------------------------------------------------------------------
	void UpdatePlayerCharacter(int playerId, string characterId)
	{
		string accountId = ELIFE_PlayerIdentity.GetInstance().GetPlayerUid(playerId);
		
		if (m_mSelectedCharacters.Contains(accountId))
		{
			m_mSelectedCharacters.Remove(accountId);
		}
		
		// Get character by characterId
		CharacterDto selectedCharacter;
		foreach (CharacterDto character : m_mPlayerCharacters.Get(accountId))
		{
			if (character.id == characterId)
			{
				selectedCharacter = character;
			}
		}
		
		if (!selectedCharacter)
			return;
		
		m_mSelectedCharacters.Insert(accountId, selectedCharacter);
	}
	
	//------------------------------------------------------------------------------------------------
	static ELIFE_PersistenceManager GetInstance()
	{
		return s_PersistenceManagerInstance;
	}
	//------------------------------------------------------------------------------------------------
	static void Initialize()
	{
		s_PersistenceManagerInstance = new ELIFE_PersistenceManager();
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_PersistenceManager()
	{
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PersistenceManager()
	{
	}

}
