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
	//! Delivery is at-least-once, so a poll can re-deliver a message this thread already holds.
	bool HasMessage(string id)
	{
		foreach (ELIFE_MessageDto message : messages)
		{
			if (message && message.messageId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Oldest first, the order a conversation reads in. sentAt is UTC ISO-8601, so string order is
	//! chronological order. Insertion sort - a thread holds far too few messages for anything else to
	//! pay for itself, and a merged thread is almost always already sorted.
	void SortMessages()
	{
		int count = messages.Count();
		for (int i = 1; i < count; i++)
		{
			ELIFE_MessageDto current = messages.Get(i);
			int j = i - 1;

			while (j >= 0 && messages.Get(j).sentAt > current.sentAt)
			{
				messages.Set(j + 1, messages.Get(j));
				j--;
			}

			messages.Set(j + 1, current);
		}
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
	//! Deep copy - see ELIFE_MessageDto.Copy() for why the merge can't share references.
	ELIFE_ThreadDisplayDto Copy()
	{
		ELIFE_ThreadDisplayDto copy = new ELIFE_ThreadDisplayDto();
		copy.threadId = threadId;
		copy.unreadCount = unreadCount;
		copy.lastMessageAt = lastMessageAt;
		copy.displayName = displayName;

		foreach (string participant : participants)
			copy.participants.Insert(participant);

		foreach (ELIFE_MessageDto message : messages)
			copy.messages.Insert(message.Copy());

		return copy;
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
