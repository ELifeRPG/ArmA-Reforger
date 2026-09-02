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

	//------------------------------------------------------------------------------------------------
	//! contactId stays real (list key, reveals nothing) - number/displayName are the actual private info.
	ELIFE_ContactDto Redact()
	{
		ELIFE_ContactDto redacted = new ELIFE_ContactDto();
		redacted.contactId = contactId;
		redacted.number = ELIFE_DataRedactor.RedactDigits(number);
		redacted.displayName = ELIFE_DataRedactor.RedactText(displayName);
		return redacted;
	}
}
