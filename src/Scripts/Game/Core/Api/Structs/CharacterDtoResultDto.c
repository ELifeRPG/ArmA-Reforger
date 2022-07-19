class CharacterDtoResultDto : JsonApiStruct
{
    ref array<ref MessageDto> messages = {};
    CharacterDto data;
	
	void CharacterDtoResultDto()
	{
		StartArray("messages");
		RegV("data");
	}
}
