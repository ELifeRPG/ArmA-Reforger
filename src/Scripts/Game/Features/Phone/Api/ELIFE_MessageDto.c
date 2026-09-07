//------------------------------------------------------------------------------------------------
class ELIFE_MessageDto : JsonApiStruct
{
	string messageId;
	string from;
	string body;
	string sentAt;
	bool isOutbound;

	void ELIFE_MessageDto()
	{
		RegV("messageId");
		RegV("from");
		RegV("body");
		RegV("sentAt");
		RegV("isOutbound");
	}

	//------------------------------------------------------------------------------------------------
	//! messageId/sentAt/isOutbound stay real - only from/body identify who said what.
	ELIFE_MessageDto Redact()
	{
		ELIFE_MessageDto redacted = new ELIFE_MessageDto();
		redacted.messageId = messageId;
		redacted.from = ELIFE_DataRedactor.RedactDigits(from);
		redacted.body = ELIFE_DataRedactor.RedactText(body);
		redacted.sentAt = sentAt;
		redacted.isOutbound = isOutbound;
		return redacted;
	}
}
