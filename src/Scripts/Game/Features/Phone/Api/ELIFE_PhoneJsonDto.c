//------------------------------------------------------------------------------------------------
//! Common base for phone DTOs that round-trip a raw JSON response. Calling Pack()/AsString() again after ExpandFromRAW() re-enters the struct's registered-variable hierarchy and trips the engine's "JsonApiStruct :: Operation called twice" warning, so the raw text is stashed at unpack time and handed back verbatim instead (see ELIFE_PhoneGadgetComponent.OnDataFetched()). Only for that passthrough case - a struct built fresh in script (never ExpandFromRAW'd) still packs normally.
class ELIFE_PhoneJsonDto : JsonApiStruct
{
	protected string m_sRawJson;

	//------------------------------------------------------------------------------------------------
	//! Called once, right after ExpandFromRAW(raw), with that same string.
	void StashRawJson(string raw)
	{
		m_sRawJson = raw;
	}

	//------------------------------------------------------------------------------------------------
	string GetRawJson()
	{
		return m_sRawJson;
	}
}
