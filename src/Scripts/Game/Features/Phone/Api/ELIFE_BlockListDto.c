//------------------------------------------------------------------------------------------------
//! The Messages blocklist, cut out of GetPhone()'s summary - nothing else on it is phone-app data.
class ELIFE_BlockListDto : ELIFE_PhoneRawDto
{
	ref array<string> numbers = {};

	void ELIFE_BlockListDto()
	{
		RegV("numbers");
	}

	//------------------------------------------------------------------------------------------------
	bool IsBlocked(string number)
	{
		return number != "" && numbers.Find(number) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh, packed copy with number added or removed - this one may be ExpandFromRAW'd.
	ELIFE_BlockListDto With(string number, bool blocked)
	{
		ELIFE_BlockListDto copy = new ELIFE_BlockListDto();

		foreach (string existing : numbers)
		{
			if (existing != number)
				copy.numbers.Insert(existing);
		}

		if (blocked)
			copy.numbers.Insert(number);

		copy.StashPacked();
		return copy;
	}
}
