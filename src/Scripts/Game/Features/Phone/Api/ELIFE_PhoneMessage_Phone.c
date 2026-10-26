//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_PhoneMessage
{
	//------------------------------------------------------------------------------------------------
	//! A fresh instance rather than a shared reference - the merged payload gets Pack()ed, which an
	//! ExpandFromRAW'd struct can't survive ("JsonApiStruct :: Operation called twice").
	ELIFE_PhoneMessage Copy()
	{
		ELIFE_PhoneMessage copy = new ELIFE_PhoneMessage();
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
	ELIFE_PhoneMessage Redact(map<string, string> contactNames)
	{
		ELIFE_PhoneMessage redacted = new ELIFE_PhoneMessage();
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
