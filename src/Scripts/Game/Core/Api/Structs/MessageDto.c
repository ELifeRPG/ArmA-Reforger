class MessageDto : JsonApiStruct
{
    MessageTypeDto type;
    string summary;
    string text;
	
	void MessageDto()
	{
		RegV("type");
		RegV("summary");
		RegV("text");
	}
}
