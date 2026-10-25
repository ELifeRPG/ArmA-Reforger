//------------------------------------------------------------------------------------------------
class ELIFE_PhoneMessageFilterRequest : ELIFE_ProfanityFilterRequest
{
	ELIFE_PhoneMessageFilter m_Filter;
	string m_sMessageId;

	//------------------------------------------------------------------------------------------------
	override protected void OnFiltered(string text)
	{
		if (m_Filter)
			m_Filter.OnBodyFiltered(m_sMessageId, text);
	}
}

//------------------------------------------------------------------------------------------------
//! Runs the owner's message bodies through the profanity filter before they're stored.
//! Results are cached by messageId, so each poll only sends messages it hasn't seen.
class ELIFE_PhoneMessageFilter
{
	ELIFE_PhoneGadgetComponent m_Phone;

	protected ref map<string, string> m_mFilteredBodies = new map<string, string>();
	protected ref array<string> m_aRequestedIds = {};

	//! Only the newest payload is released; polls carry the full merged state, so older ones add nothing.
	protected string m_sPendingJson;

	protected bool m_bCollecting;

	//------------------------------------------------------------------------------------------------
	void Filter(string json)
	{
		m_sPendingJson = json;

		ELIFE_MessageUpdates updates = new ELIFE_MessageUpdates();
		updates.ExpandFromRAW(json);

		// A platform that can't filter completes synchronously; don't release mid-loop.
		m_bCollecting = true;
		foreach (ELIFE_MessageThreadUpdate threadDto : updates.threads)
		{
			foreach (ELIFE_PhoneMessage message : threadDto.messages)
			{
				string id = message.messageId;
				if (id == "" || m_mFilteredBodies.Contains(id) || m_aRequestedIds.Contains(id))
					continue;

				m_aRequestedIds.Insert(id);

				ELIFE_PhoneMessageFilterRequest request = new ELIFE_PhoneMessageFilterRequest();
				request.m_Filter = this;
				request.m_sMessageId = id;
				ELIFE_ProfanityFilter.Filter(message.body, request);
			}
		}

		m_bCollecting = false;
		TryRelease();
	}

	//------------------------------------------------------------------------------------------------
	void OnBodyFiltered(string messageId, string body)
	{
		m_mFilteredBodies.Set(messageId, body);
		m_aRequestedIds.RemoveItem(messageId);
		TryRelease();
	}

	//------------------------------------------------------------------------------------------------
	protected void TryRelease()
	{
		if (m_bCollecting || !m_aRequestedIds.IsEmpty() || m_sPendingJson == "")
			return;

		string json = ApplyFilteredBodies(m_sPendingJson);
		m_sPendingJson = "";

		if (m_Phone)
			m_Phone.OnMessagesFiltered(json);
	}

	//------------------------------------------------------------------------------------------------
	//! Also drops cached bodies of messages no longer in the payload.
	protected string ApplyFilteredBodies(string json)
	{
		ELIFE_MessageUpdates parsed = new ELIFE_MessageUpdates();
		parsed.ExpandFromRAW(json);

		// Fresh copy: an ExpandFromRAW'd struct can't be Pack()ed.
		bool changed;
		ELIFE_MessageUpdates empty = new ELIFE_MessageUpdates();
		ELIFE_MessageUpdates updates = empty.MergedWith(parsed, changed);

		map<string, string> stillPresent = new map<string, string>();
		foreach (ELIFE_MessageThreadUpdate threadDto : updates.threads)
		{
			foreach (ELIFE_PhoneMessage message : threadDto.messages)
			{
				string filtered;
				if (!m_mFilteredBodies.Find(message.messageId, filtered))
					continue;

				message.body = filtered;
				stillPresent.Set(message.messageId, filtered);
			}
		}

		m_mFilteredBodies = stillPresent;

		updates.Pack();
		return updates.AsString();
	}
}
