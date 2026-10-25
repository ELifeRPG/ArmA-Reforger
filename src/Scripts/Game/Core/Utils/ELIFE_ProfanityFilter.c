//------------------------------------------------------------------------------------------------
//! Subclass and override OnFiltered; start it with ELIFE_ProfanityFilter.Filter().
class ELIFE_ProfanityFilterRequest : SCR_ScriptProfanityFilterRequestCallback
{
	//------------------------------------------------------------------------------------------------
	//! Called once, with the original text when the platform can't filter.
	protected void OnFiltered(string text);

	//------------------------------------------------------------------------------------------------
	//! Same post-processing as vanilla chat (SCR_ChatPanelManager.OnMessageFiltered).
	override void OnFilteredResult()
	{
		super.OnFilteredResult();

		string text = SCR_StringHelper.Join("\n", m_FilteredTexts, true);
		if (GetGame().GetPlatformService().GetLocalPlatformKind() == PlatformKind.XBOX)
		{
			string censored;
			SCR_ProfaneFilter.ReplaceProfanities(text, censored);
			text = censored;
		}

		Complete(text);
	}

	//------------------------------------------------------------------------------------------------
	void Complete(string text)
	{
		OnFiltered(text);
		ELIFE_ProfanityFilter.Release(this);
	}
}

//------------------------------------------------------------------------------------------------
//! Platform profanity filter for player-written text shown outside the chat log, which is the
//! only place vanilla filters.
class ELIFE_ProfanityFilter
{
	//! Held until the async result arrives, otherwise the request is collected.
	protected static ref array<ref ELIFE_ProfanityFilterRequest> s_aPending = {};

	//------------------------------------------------------------------------------------------------
	//! Like vanilla chat, completes with the original text when the platform can't filter.
	static void Filter(string text, notnull ELIFE_ProfanityFilterRequest request)
	{
		s_aPending.Insert(request);

		array<string> texts = {text};
		if (!GetGame().GetPlatformService().FilterProfanityAsync(texts, request))
			request.Complete(text);
	}

	//------------------------------------------------------------------------------------------------
	static void Release(ELIFE_ProfanityFilterRequest request)
	{
		s_aPending.RemoveItem(request);
	}
}
