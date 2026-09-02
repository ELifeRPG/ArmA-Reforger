//------------------------------------------------------------------------------------------------
class ELIFE_MessageDto : JsonApiStruct
{
	string id;
	string from;
	string body;
	string sentAt;
	bool isOutbound;

	void ELIFE_MessageDto()
	{
		RegV("id");
		RegV("from");
		RegV("body");
		RegV("sentAt");
		RegV("isOutbound");
	}

	//------------------------------------------------------------------------------------------------
	//! id/sentAt/isOutbound stay real (timing/structure only) - from/body identify who said what.
	ELIFE_MessageDto Redact()
	{
		ELIFE_MessageDto redacted = new ELIFE_MessageDto();
		redacted.id = id;
		redacted.from = ELIFE_DataRedactor.RedactDigits(from);
		redacted.body = ELIFE_DataRedactor.RedactText(body);
		redacted.sentAt = sentAt;
		redacted.isOutbound = isOutbound;
		return redacted;
	}
}
