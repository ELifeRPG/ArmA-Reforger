//------------------------------------------------------------------------------------------------
//! Wraps the Bridge's bare-array contacts response as {"items": <raw>} so JsonApiStruct can parse it.
class ELIFE_ContactListDto : ELIFE_PhoneRawDto
{
	ref array<ref ELIFE_ContactSummary> items = {};

	void ELIFE_ContactListDto()
	{
		RegV("items");
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
