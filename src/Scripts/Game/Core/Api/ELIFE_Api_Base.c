//------------------------------------------------------------------------------------------------
//! Thin wrapper around Enfusion's RestApi, targeting the local Bridge
class ELIFE_Api
{
	protected static ref ELIFE_Api s_ELifeApiInstance;
	protected static string serverURL;

	protected const string CONFIG_FILE_PATH = "$profile:ELifeRPG.json";
	protected const string WORKBENCH_DEFAULT_SERVER_URL = "http://127.0.0.1:5200/";

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
	protected string ParseServerUrlFromProfile()
	{
		if (!FileIO.FileExist(CONFIG_FILE_PATH))
		{
#ifdef WORKBENCH
			return WORKBENCH_DEFAULT_SERVER_URL;
#else
			return "";
#endif
		}

		ELIFE_ApiConfigDto config = new ELIFE_ApiConfigDto();
		config.ExpandFromRAW(SCR_FileIOHelper.GetFileStringContent(CONFIG_FILE_PATH));

		return config.serverUrl;
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
