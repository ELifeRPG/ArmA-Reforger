class ELIFE_PlayerIdentity
{
	protected static ref ELIFE_PlayerIdentity s_PlayerIdentityInstance;
	
	protected ref map<int, string> m_PlayerNames;
	protected ref map<int, string> m_PlayerUids;
	
	protected ref ScriptInvoker Event_OnIdentityRetrieved = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	static ELIFE_PlayerIdentity GetInstance()
	{		
		return s_PlayerIdentityInstance;
	}
	//------------------------------------------------------------------------------------------------
	static void Initialize()
	{		
		s_PlayerIdentityInstance = new ELIFE_PlayerIdentity();
	}
	
	//------------------------------------------------------------------------------------------------
	string GetPlayerUid(int playerRepId)
	{
		return m_PlayerUids.Get(playerRepId);
	}
	
	//------------------------------------------------------------------------------------------------
	string GetPlayerName(int playerRepId)
	{
		return m_PlayerNames.Get(playerRepId);
	}
	
	//------------------------------------------------------------------------------------------------
	string GetPlayerNameFromPlayerUid(string playerUid)
	{
		return GetPlayerName(GetPlayerRepIdFromPlayerUid(playerUid));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Returns the replication id of the given playerUid. 0 if player can't be found
	int GetPlayerRepIdFromPlayerUid(string playerUid)
	{
		return m_PlayerUids.GetKeyByValue(playerUid);
	}
	
	//------------------------------------------------------------------------------------------------
	void TryGetPlayerUid(int playerRepId)
	{
#ifdef WORKBENCH
		string playerUid = "01234567-89ab-cdef-0123-456789abcdef";
#else
		string playerUid = GetGame().GetBackendApi().GetPlayerUID(playerRepId);
#endif
		
		if (playerUid.IsEmpty())
		{
			// PlayerUid not found
			
			// If player is no longer in m_PlayerNames => Disconnected
			if (!m_PlayerNames.Contains(playerRepId))
				return;
			
			// Retry TryGetPlayerUid
			GetGame().GetCallqueue().CallLater(TryGetPlayerUid, 5, false, playerRepId);
		}
		else
		{
			// PlayerUid found
			m_PlayerUids.Set(playerRepId, playerUid);
			
			ELIFE_PersistenceManager.GetInstance().GetCharactersForPlayer(playerUid);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	void Event_OnPlayerConnected(int playerRepId)
	{
		m_PlayerNames.Set(playerRepId, GetGame().GetPlayerManager().GetPlayerName(playerRepId));
		TryGetPlayerUid(playerRepId);
	}
	
	//------------------------------------------------------------------------------------------------
	void Event_OnPlayerDisconnected(int playerRepId)
	{
		m_PlayerNames.Remove(playerRepId);
		m_PlayerUids.Remove(playerRepId);
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_PlayerIdentity()
	{
		m_GameMode = ELIFE_GameMode.Cast(GetGame().GetGameMode());
		
		m_PlayerNames = new map<int, string>;
		m_PlayerUids = new map<int, string>;
		
		// Add events for player connect and disconnect
		m_GameMode.GetOnPlayerConnected().Insert(Event_OnPlayerConnected);
		m_GameMode.GetOnPlayerDisconnected().Insert(Event_OnPlayerDisconnected);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PlayerIdentity()
	{
	}
}