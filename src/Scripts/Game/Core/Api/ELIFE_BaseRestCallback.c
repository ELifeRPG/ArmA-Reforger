//------------------------------------------------------------------------------------------------
//! Generic RestCallback: override ExtractData() to parse the response, dispatched via SetCallback().
//! functionName must be public, and the callback must be kept alive in a ref field until it fires.
class ELIFE_BaseRestCallback : RestCallback
{
	protected ref ScriptCallQueue m_Invoker;
	protected Managed m_InvokeInstance;
	protected string m_InvokeMethodName;
	protected string m_InvokeAdditionalData;

	//------------------------------------------------------------------------------------------------
	void SetCallback(Managed instance, string functionName, string additionalData = "")
	{
		m_Invoker = new ScriptCallQueue();
		m_InvokeInstance = instance;
		m_InvokeMethodName = functionName;
		m_InvokeAdditionalData = additionalData;

		SetOnSuccess(HandleSuccess);
		SetOnError(HandleError);
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_EApiStatusCode ExtractData(string data, int dataSize, out JsonApiStruct resultData)
	{
		return ELIFE_EApiStatusCode.SUCCESS;
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleSuccess()
	{
		string data = GetData();
		JsonApiStruct resultData;

		ELIFE_EApiStatusCode status = ExtractData(data, data.Length(), resultData);

		InvokeCallback(status, resultData);
	}

	//------------------------------------------------------------------------------------------------
	protected void HandleError()
	{
		Print(string.Format("ELIFE_BaseRestCallback | request failed, RestResult=%1 HTTP=%2", GetRestResult(), GetHttpCode()), LogLevel.ERROR);

		InvokeCallback(ELIFE_EApiStatusCode.ERROR, null);
	}

	//------------------------------------------------------------------------------------------------
	protected void InvokeCallback(ELIFE_EApiStatusCode statusCode, JsonApiStruct data)
	{
		if (m_Invoker && m_InvokeInstance && m_InvokeMethodName)
		{
			if (m_InvokeAdditionalData != "")
				m_Invoker.CallByName(m_InvokeInstance, m_InvokeMethodName, statusCode, data, m_InvokeAdditionalData);
			else
				m_Invoker.CallByName(m_InvokeInstance, m_InvokeMethodName, statusCode, data);

			m_Invoker.Tick(1);
		}
	}
}

//------------------------------------------------------------------------------------------------
enum ELIFE_EApiStatusCode
{
	SUCCESS,
	ERROR
}
