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
	protected RestContext GetElifeApi()
	{
		return GetGame().GetRestApi().GetContext(serverURL);
	}

	//------------------------------------------------------------------------------------------------
	protected string ParseServerUrlFromProfile()
	{
#ifdef WORKBENCH
		return "http://localhost:5064/";
#endif
		//Check for a saved persistent player ID			
		if (FileIO.FileExist(CONFIG_FILE_PATH))
		{
			//File exists, use it
			FileHandle f = FileIO.OpenFile(CONFIG_FILE_PATH, FileMode.READ);
			if (f){
				//f.FGets(m_sPersistentID);
				f.CloseFile();
				
				return "test";
			}
		}
		
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	void ELIFE_Api()
	{
		serverURL = ParseServerUrlFromProfile();
		
		if (serverURL == "")
		{
			Print("ELIFE_Api | ServerUrl not configured.");
			
			GetGame().RequestClose();
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_Api()
	{
	}
}