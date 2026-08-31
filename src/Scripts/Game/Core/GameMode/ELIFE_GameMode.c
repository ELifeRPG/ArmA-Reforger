[EntityEditorProps(category: "ELifeRPG/GameMode", description: "Roleplay gamemode")]
class ELIFE_GameModeClass : SCR_BaseGameModeClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_GameMode : SCR_BaseGameMode
{
	protected ref ELIFE_BaseRestCallback m_BridgeConnectivityCallback;
	protected int m_iBridgeCheckAttempts;

	protected const int BRIDGE_CHECK_MAX_ATTEMPTS = 5;
	protected const int BRIDGE_CHECK_RETRY_DELAY_MS = 2000;

	//------------------------------------------------------------------------------------------------
	static ELIFE_GameMode GetInstance()
	{
		return ELIFE_GameMode.Cast(GetGame().GetGameMode());
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameStart()
	{
		super.OnGameStart();

		if (!Replication.IsServer())
			return;

		ELIFE_Api.Initialize();
		CheckBridgeConnectivity();
	}

	//------------------------------------------------------------------------------------------------
	//! In Workbench this just warns; on a real server it retries and refuses to start on failure.
	protected void CheckBridgeConnectivity()
	{
		m_iBridgeCheckAttempts++;

		m_BridgeConnectivityCallback = new ELIFE_BaseRestCallback();
		m_BridgeConnectivityCallback.SetCallback(this, "OnBridgeConnectivityResult");
		ELIFE_Api.GetInstance().GetElifeApi().GET(m_BridgeConnectivityCallback, "health");
	}

	//------------------------------------------------------------------------------------------------
	void OnBridgeConnectivityResult(ELIFE_EApiStatusCode status, JsonApiStruct data)
	{
		if (status == ELIFE_EApiStatusCode.SUCCESS)
		{
			Print("ELIFE_GameMode | Bridge connectivity check: OK", LogLevel.NORMAL);
			return;
		}

#ifdef WORKBENCH
		Print("ELIFE_GameMode | Bridge connectivity check: FAILED - is the Bridge running?", LogLevel.WARNING);
#else
		if (m_iBridgeCheckAttempts < BRIDGE_CHECK_MAX_ATTEMPTS)
		{
			Print(string.Format("ELIFE_GameMode | Bridge unreachable, retrying (%1/%2)...", m_iBridgeCheckAttempts, BRIDGE_CHECK_MAX_ATTEMPTS), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(CheckBridgeConnectivity, BRIDGE_CHECK_RETRY_DELAY_MS, false);
		}
		else
		{
			Print("ELIFE_GameMode | Bridge unreachable after retries - refusing to start.", LogLevel.ERROR);
			GetGame().RequestClose();
		}
#endif
	}
}
