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
	//! A fresh, packed copy with one contact renamed - this one may be ExpandFromRAW'd.
	ELIFE_ContactListDto WithName(string contactId, string displayName)
	{
		ELIFE_ContactListDto copy = new ELIFE_ContactListDto();

		foreach (ELIFE_ContactSummary contact : items)
		{
			ELIFE_ContactSummary entry = new ELIFE_ContactSummary();
			entry.contactId = contact.contactId;
			entry.number = contact.number;
			entry.displayName = contact.displayName;

			if (contact.contactId == contactId)
				entry.displayName = displayName;

			copy.items.Insert(entry);
		}

		copy.StashPacked();
		return copy;
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
