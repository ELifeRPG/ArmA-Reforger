//------------------------------------------------------------------------------------------------
//! Minimal JSON escaping for hand-built request bodies - needed for player-typed text (e.g. a contact's display name) since other interpolated values (UUIDs, PINs, bools) can't contain characters JSON cares about. JsonApiStruct's Pack()/AsString() isn't used for outgoing bodies: it's a round-trip serialiser and what it emits for a hand-populated struct gets rejected by the Bridge with 400.
class ELIFE_Json
{
	//------------------------------------------------------------------------------------------------
	static string EscapeString(string value)
	{
		string result = "";
		int length = value.Length();

		for (int i = 0; i < length; i++)
		{
			string character = value.Substring(i, 1);

			if (character == "\"")
				result += "\\\"";
			else if (character == "\\")
				result += "\\\\";
			else if (character == "\n")
				result += "\\n";
			else if (character == "\r")
				result += "\\r";
			else if (character == "\t")
				result += "\\t";
			else
				result += character;
		}

		return result;
	}
}
