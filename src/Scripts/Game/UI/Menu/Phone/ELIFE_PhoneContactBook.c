//------------------------------------------------------------------------------------------------
//! Turns a subscriber number into the name the owner saved it under, shared across every screen that shows one (Messages, home cards, lock notifications) so a number never resolves differently in two places. Caches the parsed contact list keyed on the raw JSON so a thread list resolving many rows doesn't re-parse it per row.
class ELIFE_PhoneContactBook
{
	protected static string s_sCachedJson;
	protected static ref ELIFE_ContactListDto s_List;

	//------------------------------------------------------------------------------------------------
	//! The saved name for a number, or the number itself when it is not in Contacts. Never empty for
	//! a non-empty number - an unsaved number is still the truest thing that can be shown for one.
	static string NameFor(ELIFE_PhoneGadgetComponent phone, string number)
	{
		if (number == "")
			return "";

		ELIFE_ContactDto contact = FindByNumber(phone, number);
		if (contact && contact.displayName != "")
			return contact.displayName;

		return number;
	}

	//------------------------------------------------------------------------------------------------
	//! The saved name for a contactId. This is the bystander-safe lookup: contactId stays real on
	//! the redacted stream, displayName stays the name the owner saved, and the number does not
	//! have to be known (or matched) at all. Empty when this machine has no such contact.
	static string NameForId(ELIFE_PhoneGadgetComponent phone, string contactId)
	{
		ELIFE_ContactDto contact = FindById(phone, contactId);
		if (!contact)
			return "";

		if (contact.displayName != "")
			return contact.displayName;

		return contact.number;
	}

	//------------------------------------------------------------------------------------------------
	//! Title of an empty conversation opened onto a person, not a thread. contactId is tried first
	//! so a bystander mirroring `c:<id>` never has to be handed the number; the number is the
	//! owner's fallback for a chat that is not (yet) a saved contact.
	static string TitleForOpen(ELIFE_PhoneGadgetComponent phone, string contactId, string number)
	{
		if (contactId != "")
		{
			string saved = NameForId(phone, contactId);
			if (saved != "")
				return saved;
		}

		return NameFor(phone, number);
	}

	//------------------------------------------------------------------------------------------------
	//! Title of a conversation. A bystander payload already carries displayName, resolved on the
	//! server against the owner's contacts - that is the only name they can be shown, because their
	//! participant numbers have been redacted and will not key this book. The owner has no
	//! displayName on the wire and falls through to NameFor the way they always did.
	static string TitleFor(ELIFE_PhoneGadgetComponent phone, notnull ELIFE_ThreadDto threadDto)
	{
		ELIFE_ThreadDisplayDto display = ELIFE_ThreadDisplayDto.Cast(threadDto);
		if (display && display.displayName != "")
			return display.displayName;

		string result = "";

		foreach (string participant : threadDto.participants)
		{
			if (result != "")
				result += ", ";

			result += NameFor(phone, participant);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_ContactDto FindById(ELIFE_PhoneGadgetComponent phone, string contactId)
	{
		if (contactId == "")
			return null;

		ELIFE_ContactListDto list = Contacts(phone);
		if (!list)
			return null;

		foreach (ELIFE_ContactDto contact : list.items)
		{
			if (contact && contact.contactId == contactId)
				return contact;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_ContactDto FindByNumber(ELIFE_PhoneGadgetComponent phone, string number)
	{
		if (number == "")
			return null;

		ELIFE_ContactListDto list = Contacts(phone);
		if (!list)
			return null;

		foreach (ELIFE_ContactDto contact : list.items)
		{
			if (contact && contact.number == number)
				return contact;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Dropped when the phone screen closes so one player's contacts can't survive into another phone's UI in the same client session.
	static void Clear()
	{
		s_sCachedJson = "";
		s_List = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Held as the parsed list, not rebuilt per call - FindById has to return a DTO that is still
	//! alive when the caller reads number/displayName, the same reason Contacts keeps m_List.
	protected static ELIFE_ContactListDto Contacts(ELIFE_PhoneGadgetComponent phone)
	{
		if (!phone)
			return null;

		string json = phone.GetData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
		if (json == "")
		{
			Clear();
			return null;
		}

		if (s_List && json == s_sCachedJson)
			return s_List;

		s_List = new ELIFE_ContactListDto();
		s_List.ExpandFromRAW(json);
		s_sCachedJson = json;

		return s_List;
	}
}
