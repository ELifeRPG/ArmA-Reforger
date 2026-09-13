//------------------------------------------------------------------------------------------------
//! Response of GET /apps/messages/updates. Without a "since" cursor every thread comes back whole;
//! "polledAt" is the cursor for the next poll.
class ELIFE_MessageUpdatesDto : ELIFE_PhoneJsonDto
{
	string polledAt;
	ref array<ref ELIFE_ThreadDisplayDto> threads = {};

	void ELIFE_MessageUpdatesDto()
	{
		RegV("polledAt");
		RegV("threads");
	}

	//------------------------------------------------------------------------------------------------
	//! Folds a cursor poll's response (only threads with new activity, only messages since the cursor) into this cached payload; changed reports whether anything actually moved.
	ELIFE_MessageUpdatesDto MergedWith(notnull ELIFE_MessageUpdatesDto delta, out bool changed)
	{
		changed = false;

		ELIFE_MessageUpdatesDto merged = new ELIFE_MessageUpdatesDto();
		merged.polledAt = delta.polledAt;

		foreach (ELIFE_ThreadDisplayDto cached : threads)
			merged.threads.Insert(cached.Copy());

		foreach (ELIFE_ThreadDisplayDto incoming : delta.threads)
		{
			ELIFE_ThreadDisplayDto target = merged.FindThread(incoming.threadId);
			if (!target)
			{
				merged.threads.Insert(incoming.Copy());
				changed = true;
				continue;
			}

			//! Both are the API's current truth for this thread, not deltas - unreadCount especially,
			//! which can fall (read elsewhere) as well as rise.
			if (target.unreadCount != incoming.unreadCount || target.lastMessageAt != incoming.lastMessageAt)
				changed = true;

			target.unreadCount = incoming.unreadCount;
			target.lastMessageAt = incoming.lastMessageAt;

			foreach (ELIFE_MessageDto message : incoming.messages)
			{
				if (target.HasMessage(message.messageId))
					continue;

				target.messages.Insert(message.Copy());
				changed = true;
			}

			target.SortMessages();
		}

		merged.SortThreads();
		return merged;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_ThreadDisplayDto FindThread(string id)
	{
		foreach (ELIFE_ThreadDisplayDto threadDto : threads)
		{
			if (threadDto && threadDto.threadId == id)
				return threadDto;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Newest first. The API already answers in this order, but a merge can move a thread that a poll
	//! revived - and ELIFE_PhoneMessagesApp.FillIndex() groups into day buckets in a single pass, so
	//! an out-of-order thread would open a duplicate section rather than just sit in the wrong place.
	protected void SortThreads()
	{
		int count = threads.Count();
		for (int i = 1; i < count; i++)
		{
			ELIFE_ThreadDisplayDto current = threads.Get(i);
			int j = i - 1;

			while (j >= 0 && threads.Get(j).lastMessageAt < current.lastMessageAt)
			{
				threads.Set(j + 1, threads.Get(j));
				j--;
			}

			threads.Set(j + 1, current);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! contactNames is the owner's number -> saved name map, used to stamp each thread's displayName.
	ELIFE_MessageUpdatesDto Redact(map<string, string> contactNames)
	{
		ELIFE_MessageUpdatesDto redacted = new ELIFE_MessageUpdatesDto();
		redacted.polledAt = polledAt;

		foreach (ELIFE_ThreadDisplayDto threadDto : threads)
			redacted.threads.Insert(threadDto.Redact(contactNames));

		return redacted;
	}
}
