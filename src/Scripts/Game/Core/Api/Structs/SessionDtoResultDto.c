class SessionDtoResultDto : JsonApiStruct
{
    ref array<ref MessageDto> messages = {};
    SessionDto data;
	
	void SessionDtoResultDto()
	{
		StartArray("messages");
		RegV("data");
	}
}
