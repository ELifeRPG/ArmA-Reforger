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

	//------------------------------------------------------------------------------------------------
	//! threadId/unreadCount/lastMessageAt stay real - only participants/messages identify anyone.
	ELIFE_ThreadDto Redact()
	{
		ELIFE_ThreadDto redacted = new ELIFE_ThreadDto();
		redacted.threadId = threadId;
		redacted.unreadCount = unreadCount;
		redacted.lastMessageAt = lastMessageAt;

		foreach (string participant : participants)
			redacted.participants.Insert(ELIFE_DataRedactor.RedactDigits(participant));

		foreach (ELIFE_MessageDto message : messages)
			redacted.messages.Insert(message.Redact());

		return redacted;
	}
}
