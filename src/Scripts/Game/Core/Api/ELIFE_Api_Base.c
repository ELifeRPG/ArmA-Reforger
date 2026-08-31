//------------------------------------------------------------------------------------------------
//! Thin wrapper around Enfusion's RestApi, targeting the local Bridge
class ELIFE_Api
{
	protected static ref ELIFE_Api s_ELifeApiInstance;
	protected static string serverURL;

	protected const string CONFIG_FILE_PATH = "$profile:ELifeRPG.json";

	//------------------------------------------------------------------------------------------------
	static ELIFE_Api GetInstance()
	{
		return s_ELifeApiInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Initialize()
	{
		s_ELifeApiInstance = new ELIFE_Api();
	}

	//------------------------------------------------------------------------------------------------
	RestContext GetElifeApi()
	{
		return GetGame().GetRestApi().GetContext(serverURL);
	}

	//------------------------------------------------------------------------------------------------
	//! Hardcoded to the dev Bridge for now - TODO Step 2: read CONFIG_FILE_PATH
	protected string ParseServerUrlFromProfile()
	{
		return "http://127.0.0.1:5200/";
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_Api()
	{
		serverURL = ParseServerUrlFromProfile();

		if (serverURL != "")
			return;

#ifdef WORKBENCH
		Print("ELIFE_Api | ServerUrl not configured.", LogLevel.WARNING);
#else
		Print("ELIFE_Api | ServerUrl not configured - refusing to start.", LogLevel.ERROR);
		GetGame().RequestClose();
#endif
	}
}
