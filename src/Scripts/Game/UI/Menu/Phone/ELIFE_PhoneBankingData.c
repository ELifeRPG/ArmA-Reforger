//------------------------------------------------------------------------------------------------
//! Money formatting for the Banking app and the home balance card. Amounts arrive from the Bridge as floats.
class ELIFE_PhoneBankingService
{
	protected static const string IBAN_COUNTRY = "EL";
	protected static const int IBAN_BANK_CODE_LENGTH = 4;
	protected static const int IBAN_ACCOUNT_LENGTH = 10;
	protected static const int IBAN_GROUP_LENGTH = 4;
	protected static const string IBAN_LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	protected static const string IBAN_CHECK_VALUES = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	protected static const string HEX_DIGITS = "0123456789abcdef";

	//------------------------------------------------------------------------------------------------
	static int ToCents(float amount)
	{
		return Math.Round(amount * 100);
	}

	//------------------------------------------------------------------------------------------------
	static string FormatMoney(int cents)
	{
		bool negative = cents < 0;
		if (negative)
			cents = -cents;

		int major = cents / 100;
		int minor = cents % 100;
		string minorText = minor.ToString();
		if (minor < 10)
			minorText = "0" + minorText;

		string sign = "";
		if (negative)
			sign = "-";

		//! Unit suffixed, not prefixed, so the sign stays leading and figures keep a common left edge.
		return sign + InsertThousands(major) + "." + minorText + " " + ELIFE_PhoneStyle.CURRENCY_SYMBOL;
	}

	//------------------------------------------------------------------------------------------------
	//! Display only, until the backend issues these: EL, check digits, 4-letter bank code, 10-digit account.
	//! The code comes from the bank's id, so a renamed bank keeps it. Not usable as an API identifier.
	static string ToIban(string number, string bankId)
	{
		if (number.Length() < IBAN_ACCOUNT_LENGTH || bankId.Length() < IBAN_BANK_CODE_LENGTH * 2)
			return number;

		string bankCode = "";
		for (int i = 0; i < IBAN_BANK_CODE_LENGTH; i++)
		{
			int high = HEX_DIGITS.IndexOf(bankId.Substring(i * 2, 1));
			int low = HEX_DIGITS.IndexOf(bankId.Substring(i * 2 + 1, 1));
			bankCode += IBAN_LETTERS.Substring((high * 16 + low) % IBAN_LETTERS.Length(), 1);
		}

		string account = number.Substring(number.Length() - IBAN_ACCOUNT_LENGTH, IBAN_ACCOUNT_LENGTH);
		int check = 98 - IbanRemainder(bankCode + account + IBAN_COUNTRY + "00");

		string checkText = check.ToString();
		if (check < 10)
			checkText = "0" + checkText;

		return IBAN_COUNTRY + checkText + bankCode + account;
	}

	//------------------------------------------------------------------------------------------------
	//! MOD-97, where a letter counts as a two-digit number: A is 10, B is 11.
	protected static int IbanRemainder(string text)
	{
		int remainder;
		for (int i = 0; i < text.Length(); i++)
		{
			int value = IBAN_CHECK_VALUES.IndexOf(text.Substring(i, 1));
			if (value >= 10)
				remainder = (remainder * 100 + value) % 97;
			else
				remainder = (remainder * 10 + value) % 97;
		}

		return remainder;
	}

	//------------------------------------------------------------------------------------------------
	//! Groups of four, the way an IBAN is printed.
	static string GroupIban(string iban)
	{
		string grouped = "";
		for (int i = 0; i < iban.Length(); i += IBAN_GROUP_LENGTH)
		{
			if (i > 0)
				grouped += " ";

			int length = iban.Length() - i;
			if (length > IBAN_GROUP_LENGTH)
				length = IBAN_GROUP_LENGTH;

			grouped += iban.Substring(i, length);
		}

		return grouped;
	}

	//------------------------------------------------------------------------------------------------
	//! For amounts that can only be whole tolars, such as cash.
	static string FormatWholeMoney(int tolars)
	{
		return InsertThousands(tolars) + " " + ELIFE_PhoneStyle.CURRENCY_SYMBOL;
	}

	//------------------------------------------------------------------------------------------------
	static string FormatSignedMoney(int cents)
	{
		if (cents > 0)
			return "+" + FormatMoney(cents);

		return FormatMoney(cents);
	}

	//------------------------------------------------------------------------------------------------
	protected static string InsertThousands(int major)
	{
		if (major < 1000)
			return major.ToString();

		string result = "";
		while (major >= 1000)
		{
			int group = major % 1000;
			major = major / 1000;

			string groupText = group.ToString();
			if (group < 10)
				groupText = "00" + groupText;
			else if (group < 100)
				groupText = "0" + groupText;

			if (result == "")
				result = groupText;
			else
				result = groupText + " " + result;
		}

		return major.ToString() + " " + result;
	}
}
