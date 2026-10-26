//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_ContactSummary
{
	//! contactId and displayName stay real; only the number is private info for a bystander's view.
	ELIFE_ContactSummary Redact()
	{
		ELIFE_ContactSummary redacted = new ELIFE_ContactSummary();
		redacted.contactId = contactId;
		redacted.number = ELIFE_DataRedactor.RedactPhoneNumber();
		redacted.displayName = displayName;
		return redacted;
	}
}
