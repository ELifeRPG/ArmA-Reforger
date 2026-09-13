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
	//! A fresh instance rather than a shared reference - the merged payload gets Pack()ed, which an
	//! ExpandFromRAW'd struct can't survive (see ELIFE_PhoneJsonDto).
	ELIFE_MessageDto Copy()
	{
		ELIFE_MessageDto copy = new ELIFE_MessageDto();
		copy.messageId = messageId;
		copy.from = from;
		copy.body = body;
		copy.sentAt = sentAt;
		copy.isOutbound = isOutbound;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! from is written as a display name here (not a lookup key), since a bystander can't match a
	//! redacted number against the real contact list; body stays private text either way.
	ELIFE_MessageDto Redact(map<string, string> contactNames)
	{
		ELIFE_MessageDto redacted = new ELIFE_MessageDto();
		redacted.messageId = messageId;
		redacted.from = DisplayFrom(contactNames);
		redacted.body = ELIFE_DataRedactor.RedactText(body);
		redacted.sentAt = sentAt;
		redacted.isOutbound = isOutbound;
		return redacted;
	}

	//------------------------------------------------------------------------------------------------
	protected string DisplayFrom(map<string, string> contactNames)
	{
		if (from == "")
			return "";

		string saved;
		if (contactNames && contactNames.Find(from, saved) && saved != "")
			return saved;

		return ELIFE_DataRedactor.RedactPhoneNumber();
	}
}
