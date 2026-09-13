//------------------------------------------------------------------------------------------------
//! Resolves a subscriber number to its saved contact name, shared by every screen so a number never resolves differently in two places.
class ELIFE_PhoneContactBook
{
	//! Char rank table for CompareNames() - Enforce Script has no string comparison operator, so sorting goes by index into this alphabet instead.
	protected static const string SORT_ALPHABET = "abcdefghijklmnopqrstuvwxyz";

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
	//! Bystander-safe lookup by contactId (stays real on the redacted stream, unlike the number). Empty when this machine has no such contact.
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
	//! Title of an empty conversation opened onto a person, not a thread - contactId is tried first so a bystander mirroring `c:<id>` never needs the number.
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
	//! Title of a conversation - a bystander payload carries a server-resolved displayName; the owner has none and falls through to NameFor per participant.
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
	//! The name shown for a contact row - the saved display name, or the bare number for one without.
	static string DisplayName(notnull ELIFE_ContactDto contact)
	{
		if (contact.displayName != "")
			return contact.displayName;

		return contact.number;
	}

	//------------------------------------------------------------------------------------------------
	//! contactId is the stable key; falls back to the number for a contact without one (FindById
	//! accepts either).
	static string KeyFor(notnull ELIFE_ContactDto contact)
	{
		if (contact.contactId != "")
			return contact.contactId;

		return contact.number;
	}

	//------------------------------------------------------------------------------------------------
	//! Selection sort by last name - an address book is small enough that O(n^2) doesn't matter.
	//! Shared by Contacts' A-Z index and Messages' Add picker so both group and order identically.
	static array<ref ELIFE_ContactDto> SortedByLastName(ELIFE_ContactListDto list)
	{
		array<ref ELIFE_ContactDto> items = {};
		if (list)
		{
			foreach (ELIFE_ContactDto contact : list.items)
			{
				if (contact)
					items.Insert(contact);
			}
		}

		array<bool> taken = {};
		for (int i = 0; i < items.Count(); i++)
			taken.Insert(false);

		array<ref ELIFE_ContactDto> sorted = {};

		for (int pass = 0; pass < items.Count(); pass++)
		{
			int bestIndex = -1;
			string bestKey = "";

			for (int i = 0; i < items.Count(); i++)
			{
				if (taken[i])
					continue;

				string key = LastName(items.Get(i));
				if (bestIndex < 0 || CompareNames(key, bestKey) < 0)
				{
					bestIndex = i;
					bestKey = key;
				}
			}

			if (bestIndex < 0)
				break;

			taken[bestIndex] = true;
			sorted.Insert(items.Get(bestIndex));
		}

		return sorted;
	}

	//------------------------------------------------------------------------------------------------
	//! Last whitespace-separated word of the display name - "Jane Doe" sorts/groups by "Doe". A
	//! one-word name (or a bare number) has no real last name, so the whole thing stands in for it.
	static string LastName(notnull ELIFE_ContactDto contact)
	{
		string name = DisplayName(contact);

		array<string> words = {};
		name.Split(" ", words, true);

		if (words.IsEmpty())
			return name;

		return words.Get(words.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	static string GroupLetter(notnull ELIFE_ContactDto contact)
	{
		string last = LastName(contact);
		if (last.Length() == 0)
			return "#";

		string letter = last.Substring(0, 1);
		letter.ToUpper();
		return letter;
	}

	//------------------------------------------------------------------------------------------------
	//! strcmp()-style contract: negative if `a` sorts before `b`, positive if after, 0 if equal.
	protected static int CompareNames(string a, string b)
	{
		string la = a;
		la.ToLower();

		string lb = b;
		lb.ToLower();

		int lenA = la.Length();
		int lenB = lb.Length();

		int len = lenA;
		if (lenB < len)
			len = lenB;

		for (int i = 0; i < len; i++)
		{
			string ca = la.Substring(i, 1);
			string cb = lb.Substring(i, 1);

			if (ca == cb)
				continue;

			return CharRank(ca) - CharRank(cb);
		}

		return lenA - lenB;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CharRank(string lowerChar)
	{
		int rank = SORT_ALPHABET.IndexOf(lowerChar);
		if (rank < 0)
			return SORT_ALPHABET.Length();

		return rank;
	}

	//------------------------------------------------------------------------------------------------
	//! Cached parsed list so a returned DTO stays alive after the call, keyed on the raw JSON to skip re-parsing when it hasn't changed.
	static ELIFE_ContactListDto Contacts(ELIFE_PhoneGadgetComponent phone)
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
