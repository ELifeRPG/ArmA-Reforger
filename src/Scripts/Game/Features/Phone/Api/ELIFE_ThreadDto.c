//------------------------------------------------------------------------------------------------
//! Covers both API shapes: the thread-list summary (no messages) and single-thread detail (with
//! messages) - "messages" just stays empty when the response doesn't include it.
class ELIFE_ThreadDto : JsonApiStruct
{
	string id;
	ref array<string> participants = {};
	int unreadCount;
	string lastMessageAt;
	ref array<ref ELIFE_MessageDto> messages = {};

	void ELIFE_ThreadDto()
	{
		RegV("id");
		RegV("participants");
		RegV("unreadCount");
		RegV("lastMessageAt");
		RegV("messages");
	}

	//------------------------------------------------------------------------------------------------
	//! id/unreadCount/lastMessageAt stay real (activity signal only) - participants/messages identify who.
	ELIFE_ThreadDto Redact()
	{
		ELIFE_ThreadDto redacted = new ELIFE_ThreadDto();
		redacted.id = id;
		redacted.unreadCount = unreadCount;
		redacted.lastMessageAt = lastMessageAt;

		foreach (string participant : participants)
			redacted.participants.Insert(ELIFE_DataRedactor.RedactDigits(participant));

		foreach (ELIFE_MessageDto message : messages)
			redacted.messages.Insert(message.Redact());

		return redacted;
	}
}
