#include <YSI\y_hooks>


hook OnScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, _T<M,O,D,1>, sizeof(data), data);

	printf("Module 1 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, _T<M,O,D,1>, sizeof(data), data);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new
			len = strlen(cmd) - 5,
			data[32];

		strmid(data, cmd, 5, strlen(cmd));

		modio_push(FILE_RUNTIME, _T<R,C,O,N>, len, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[32];

		modio_read(FILE_RUNTIME, _T<R,C,O,N>, sizeof(data), data);

		printf("RCON: %s", data);

		return 1;
	}

	return 0;
}
