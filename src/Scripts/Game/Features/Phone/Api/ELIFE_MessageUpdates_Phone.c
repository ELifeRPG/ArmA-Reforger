//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_MessageUpdates
{
	//------------------------------------------------------------------------------------------------
	//! Folds a poll's response (only threads with unretrieved messages) into this cached payload;
	//! changed reports whether anything actually moved. Delivery is at-least-once — dedupe on messageId.
	ELIFE_MessageUpdates MergedWith(notnull ELIFE_MessageUpdates delta, out bool changed)
	{
		changed = false;

		ELIFE_MessageUpdates merged = new ELIFE_MessageUpdates();

		foreach (ELIFE_MessageThreadUpdate cached : threads)
			merged.threads.Insert(cached.Copy());

		foreach (ELIFE_MessageThreadUpdate incoming : delta.threads)
		{
			ELIFE_MessageThreadUpdate target = merged.FindThread(incoming.threadId);
			if (!target)
			{
				merged.threads.Insert(incoming.Copy());
				changed = true;
				continue;
			}

			//! Poll payloads only arrive for unretrieved mail — unread here is that batch's truth,
			//! not a full-thread refresh (mark-read clears the cache locally).
			if (target.unreadCount != incoming.unreadCount || target.lastMessageAt != incoming.lastMessageAt)
				changed = true;

			target.unreadCount = incoming.unreadCount;
			target.lastMessageAt = incoming.lastMessageAt;
			target.retrievedThrough = incoming.retrievedThrough;
			target.highestSequence = incoming.highestSequence;

			foreach (ELIFE_PhoneMessage message : incoming.messages)
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
	ELIFE_MessageThreadUpdate FindThread(string id)
	{
		foreach (ELIFE_MessageThreadUpdate threadDto : threads)
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
			ELIFE_MessageThreadUpdate current = threads.Get(i);
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
	ELIFE_MessageUpdates Redact(map<string, string> contactNames)
	{
		ELIFE_MessageUpdates redacted = new ELIFE_MessageUpdates();

		foreach (ELIFE_MessageThreadUpdate threadDto : threads)
			redacted.threads.Insert(threadDto.Redact(contactNames));

		return redacted;
	}
}
