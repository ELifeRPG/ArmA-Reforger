//------------------------------------------------------------------------------------------------
//! Covers every thread shape the Bridge returns - "messages" stays empty when the response omits it.
class ELIFE_ThreadDto : JsonApiStruct
{
	string threadId;
	ref array<string> participants = {};
	int unreadCount;
	string lastMessageAt;
	ref array<ref ELIFE_MessageDto> messages = {};

	void ELIFE_ThreadDto()
	{
		RegV("threadId");
		RegV("participants");
		RegV("unreadCount");
		RegV("lastMessageAt");
		RegV("messages");
	}
}

//------------------------------------------------------------------------------------------------
//! The Bridge's thread plus a title a bystander can be shown; displayName is filled in locally from
//! the owner's contacts, since a redacted participant number no longer keys into that list.
class ELIFE_ThreadDisplayDto : ELIFE_ThreadDto
{
	string displayName;

	void ELIFE_ThreadDisplayDto()
	{
		RegV("displayName");
	}

	//------------------------------------------------------------------------------------------------
	//! contactNames is number -> saved name from the owner's real contacts; a miss falls back to the fixed "unknown number" placeholder.
	ELIFE_ThreadDisplayDto Redact(map<string, string> contactNames)
	{
		ELIFE_ThreadDisplayDto redacted = new ELIFE_ThreadDisplayDto();
		redacted.threadId = threadId;
		redacted.unreadCount = unreadCount;
		redacted.lastMessageAt = lastMessageAt;
		redacted.displayName = ResolveDisplayName(contactNames);

		foreach (string participant : participants)
			redacted.participants.Insert(ELIFE_DataRedactor.RedactPhoneNumber());

		foreach (ELIFE_MessageDto message : messages)
			redacted.messages.Insert(message.Redact(contactNames));

		return redacted;
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveDisplayName(map<string, string> contactNames)
	{
		string result = "";

		foreach (string participant : participants)
		{
			if (result != "")
				result += ", ";

			string saved;
			if (contactNames && contactNames.Find(participant, saved) && saved != "")
				result += saved;
			else
				result += ELIFE_DataRedactor.RedactPhoneNumber();
		}

		return result;
	}
}
