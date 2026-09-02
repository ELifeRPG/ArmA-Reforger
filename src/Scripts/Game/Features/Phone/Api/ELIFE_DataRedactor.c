//------------------------------------------------------------------------------------------------
//! Diffuses private DTO fields for bystanders: same shape and rough visual length as the real
//! value, random content. Re-rolls on every call - fine since nothing re-renders mid-screen.
class ELIFE_DataRedactor
{
	protected const string LETTERS_LOWER = "abcdefghijklmnopqrstuvwxyz";
	protected const string LETTERS_UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	protected const string DIGITS = "0123456789";
	protected const int LENGTH_VARIANCE_PERCENT = 30;

	//------------------------------------------------------------------------------------------------
	//! Redacts free text (names, messages) word by word so it keeps a name/sentence-like rhythm.
	static string RedactText(string real)
	{
		if (real == "")
			return "";

		array<string> words = {};
		real.Split(" ", words, true);

		string result = "";
		for (int i = 0; i < words.Count(); i++)
		{
			if (i > 0)
				result += " ";
			result += RedactWord(words[i]);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Redacts numeric-looking text (phone numbers, PINs): keeps separators in place, randomizes digits.
	static string RedactDigits(string real)
	{
		string result = "";
		for (int i = 0; i < real.Length(); i++)
		{
			string ch = real.Substring(i, 1);
			if (DIGITS.IndexOf(ch) != -1)
				result += RandomChar(DIGITS);
			else
				result += ch;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RedactWord(string word)
	{
		int len = word.Length();
		if (len == 0)
			return "";

		int variance = Math.Max(1, (len * LENGTH_VARIANCE_PERCENT) / 100);
		int redactedLen = Math.Max(1, len + Math.RandomIntInclusive(-variance, variance));

		bool capitalized = LETTERS_UPPER.IndexOf(word.Substring(0, 1)) != -1;

		string result = "";
		for (int i = 0; i < redactedLen; i++)
		{
			if (i == 0 && capitalized)
				result += RandomChar(LETTERS_UPPER);
			else
				result += RandomChar(LETTERS_LOWER);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string RandomChar(string pool)
	{
		int index = Math.RandomIntInclusive(0, pool.Length() - 1);
		return pool.Substring(index, 1);
	}
}
