//------------------------------------------------------------------------------------------------
//! A phone payload that keeps the JSON text it was built from, so the gadget can cache and push it
//! without re-packing - re-packing an ExpandFromRAW'd struct warns "Operation called twice".
class ELIFE_PhoneRawDto : JsonApiStruct
{
	protected string m_sRawJson;

	//------------------------------------------------------------------------------------------------
	//! For a struct filled in script rather than parsed: packs it once and keeps the result.
	void StashPacked()
	{
		Pack();
		m_sRawJson = AsString();
	}

	//------------------------------------------------------------------------------------------------
	//! Bridge list routes return a bare JSON array, which JsonApiStruct can't parse - wrap it as "items".
	//! head is spliced in before "items", for payloads that carry more than the list.
	void ExpandFromArray(string arrayJson, string head = "")
	{
		string wrapped = "{" + head + "\"items\":" + arrayJson + "}";
		ExpandFromRAW(wrapped);
		m_sRawJson = wrapped;
	}

	//------------------------------------------------------------------------------------------------
	string GetRawJson()
	{
		return m_sRawJson;
	}
}
