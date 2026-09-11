//------------------------------------------------------------------------------------------------
class ELIFE_ContactDto : JsonApiStruct
{
	string contactId;
	string number;
	string displayName;

	void ELIFE_ContactDto()
	{
		RegV("contactId");
		RegV("number");
		RegV("displayName");
	}

	//! contactId and displayName stay real; only the number is private info for a bystander's view.
	ELIFE_ContactDto Redact()
	{
		ELIFE_ContactDto redacted = new ELIFE_ContactDto();
		redacted.contactId = contactId;
		redacted.number = ELIFE_DataRedactor.RedactPhoneNumber();
		redacted.displayName = displayName;
		return redacted;
	}
}
