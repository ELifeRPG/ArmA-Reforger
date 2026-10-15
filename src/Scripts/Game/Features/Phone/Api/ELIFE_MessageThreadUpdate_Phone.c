//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
//! displayName isn't from the Bridge: it's the bystander-copy title, resolved from the owner's contacts.
modded class ELIFE_MessageThreadUpdate
{
	string displayName;

	void ELIFE_MessageThreadUpdate()
	{
		RegV("displayName");
	}

	//------------------------------------------------------------------------------------------------
	//! Delivery is at-least-once, so a poll can re-deliver a message this thread already holds.
	bool HasMessage(string id)
	{
		foreach (ELIFE_PhoneMessage message : messages)
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
			ELIFE_PhoneMessage current = messages.Get(i);
			int j = i - 1;

			while (j >= 0 && messages.Get(j).sentAt > current.sentAt)
			{
				messages.Set(j + 1, messages.Get(j));
				j--;
			}

			messages.Set(j + 1, current);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deep copy - see ELIFE_PhoneMessage.Copy() for why the merge can't share references.
	ELIFE_MessageThreadUpdate Copy()
	{
		ELIFE_MessageThreadUpdate copy = new ELIFE_MessageThreadUpdate();
		copy.threadId = threadId;
		copy.unreadCount = unreadCount;
		copy.lastMessageAt = lastMessageAt;
		copy.retrievedThrough = retrievedThrough;
		copy.highestSequence = highestSequence;
		copy.displayName = displayName;

		foreach (string participant : participants)
			copy.participants.Insert(participant);

		foreach (ELIFE_PhoneMessage message : messages)
			copy.messages.Insert(message.Copy());

		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! contactNames is number -> saved name from the owner's real contacts; a miss falls back to the fixed "unknown number" placeholder.
	ELIFE_MessageThreadUpdate Redact(map<string, string> contactNames)
	{
		ELIFE_MessageThreadUpdate redacted = new ELIFE_MessageThreadUpdate();
		redacted.threadId = threadId;
		redacted.unreadCount = unreadCount;
		redacted.lastMessageAt = lastMessageAt;
		redacted.retrievedThrough = retrievedThrough;
		redacted.highestSequence = highestSequence;
		redacted.displayName = ResolveDisplayName(contactNames);

		foreach (string participant : participants)
			redacted.participants.Insert(ELIFE_DataRedactor.RedactPhoneNumber());

		foreach (ELIFE_PhoneMessage message : messages)
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
