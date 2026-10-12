//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array contacts response as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_ContactListDto : JsonApiStruct
{
	ref array<ref ELIFE_ContactSummary> items = {};

	//! Pushed to the owner as-is: re-packing an ExpandFromRAW'd struct warns "Operation called twice".
	protected string m_sRawJson;

	void ELIFE_ContactListDto()
	{
		RegV("items");
	}

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

	//------------------------------------------------------------------------------------------------
	ELIFE_ContactListDto Redact()
	{
		ELIFE_ContactListDto redacted = new ELIFE_ContactListDto();

		foreach (ELIFE_ContactSummary contact : items)
			redacted.items.Insert(contact.Redact());

		return redacted;
	}
}
