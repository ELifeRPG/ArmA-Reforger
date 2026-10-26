//------------------------------------------------------------------------------------------------
//! Phone logic on the generated struct; lives here so codegen doesn't overwrite it.
modded class ELIFE_MessageUpdates
{
	//! Preview messages built from a notification carry this before the notificationId.
	static const string PREVIEW_ID_PREFIX = "n:";

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
	//! The thread list is the inbox's truth: its threads, counts and order. It carries no messages, so
	//! whatever this payload already holds per thread stays.
	ELIFE_MessageUpdates WithThreadList(notnull ELIFE_MessageThreadSummaryListDto list)
	{
		ELIFE_MessageUpdates merged = new ELIFE_MessageUpdates();

		foreach (ELIFE_MessageThreadSummary summary : list.items)
		{
			if (!summary || summary.threadId == "")
				continue;

			ELIFE_MessageThreadUpdate threadDto;
			ELIFE_MessageThreadUpdate cached = FindThread(summary.threadId);
			if (cached)
			{
				threadDto = cached.Copy();
			}
			else
			{
				threadDto = new ELIFE_MessageThreadUpdate();
				threadDto.threadId = summary.threadId;
			}

			threadDto.participants.Clear();
			foreach (string participant : summary.participants)
				threadDto.participants.Insert(participant);

			threadDto.unreadCount = summary.unreadCount;
			threadDto.lastMessageAt = summary.lastMessageAt;
			merged.threads.Insert(threadDto);
		}

		merged.SortThreads();
		return merged;
	}

	//------------------------------------------------------------------------------------------------
	//! One conversation's retained history replaces what was cached for it. Every other thread keeps
	//! only its newest message, so the payload holds one full conversation at a time.
	ELIFE_MessageUpdates WithThreadDetail(notnull ELIFE_MessageThreadDetail detail)
	{
		ELIFE_MessageUpdates merged = new ELIFE_MessageUpdates();
		bool found = false;

		foreach (ELIFE_MessageThreadUpdate cached : threads)
		{
			if (cached.threadId == detail.threadId)
			{
				merged.threads.Insert(ELIFE_MessageThreadUpdate.FromDetail(detail));
				found = true;
				continue;
			}

			ELIFE_MessageThreadUpdate copy = cached.Copy();
			copy.KeepNewestOnly();
			merged.threads.Insert(copy);
		}

		if (!found)
			merged.threads.Insert(ELIFE_MessageThreadUpdate.FromDetail(detail));

		merged.SortThreads();
		return merged;
	}

	//------------------------------------------------------------------------------------------------
	//! A notification is the only place a new message's text shows before its thread is fetched, so it
	//! becomes that thread's preview until the real message replaces it.
	ELIFE_MessageUpdates WithNotificationPreviews(notnull array<ELIFE_PhoneNotification> fresh, string fullThreadId)
	{
		ELIFE_MessageUpdates merged = new ELIFE_MessageUpdates();
		foreach (ELIFE_MessageThreadUpdate cached : threads)
			merged.threads.Insert(cached.Copy());

		foreach (ELIFE_PhoneNotification notification : fresh)
		{
			if (!notification || !notification.IsMessage())
				continue;

			ELIFE_MessageThreadUpdate threadDto = merged.FindThread(notification.groupKey);
			if (!threadDto)
			{
				threadDto = new ELIFE_MessageThreadUpdate();
				threadDto.threadId = notification.groupKey;
				threadDto.participants.Insert(notification.title);
				merged.threads.Insert(threadDto);
			}

			if (threadDto.HasMessageSince(notification.occurredAt))
				continue;

			ELIFE_PhoneMessage preview = new ELIFE_PhoneMessage();
			preview.messageId = PREVIEW_ID_PREFIX + notification.notificationId;
			preview.from = notification.title;
			preview.body = notification.body;
			preview.sentAt = notification.occurredAt;
			threadDto.messages.Insert(preview);
			threadDto.SortMessages();

			if (threadDto.threadId != fullThreadId)
				threadDto.KeepNewestOnly();

			if (threadDto.lastMessageAt < notification.occurredAt)
				threadDto.lastMessageAt = notification.occurredAt;
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
