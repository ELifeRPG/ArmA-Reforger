class ELIFE_BaseRestCallback : RestCallback
{	
	protected ref ScriptCallQueue m_Invoker;
	protected Managed m_InvokeInstance;
	protected string m_InvokeMethodName;
	protected string m_InvokeAdditionalData;
	
	void SetCallback(Managed instance, string functionName, string additionalData = "")
	{
		m_Invoker = new ScriptCallQueue();
		m_InvokeInstance = instance;
		m_InvokeMethodName = functionName;
		m_InvokeAdditionalData = additionalData;
	}
	
	ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		
		return ELIFE_EApiStatusCode.SUCCESS;
	}
	
	override void OnSuccess(string data, int dataSize)
	{		
		JsonApiStruct resultData;
		
		ELIFE_EApiStatusCode status = ExtractData(data, dataSize, resultData);
		
		InvokeCallback(status, resultData);
	}
	
	override void OnError( int errorCode )
	{
		InvokeCallback(ELIFE_EApiStatusCode.ERROR, null);
	}
	
	protected void InvokeCallback(ELIFE_EApiStatusCode statusCode, JsonApiStruct data)
	{
		if (m_Invoker && m_InvokeInstance && m_InvokeMethodName)
		{
			if (m_InvokeAdditionalData != "")
			{
				m_Invoker.CallByName(m_InvokeInstance, m_InvokeMethodName, statusCode, data, m_InvokeAdditionalData);
			}
			else
			{
				m_Invoker.CallByName(m_InvokeInstance, m_InvokeMethodName, statusCode, data);
			}
			
			m_Invoker.Tick(1);
		}
	}
}

enum ELIFE_EApiStatusCode
{
	SUCCESS,
	ERROR
}