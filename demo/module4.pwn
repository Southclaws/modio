#include <YSI\y_hooks>


hook OnScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, _T<M,O,D,4>, sizeof(data), data);

	printf("Module 4 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, _T<M,O,D,4>, sizeof(data), data, .forcewrite = true);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[10];

		data = "Southclaw";

		modio_push(FILE_RUNTIME, _T<U,S,E,R>, 10, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[10];

		modio_read(FILE_RUNTIME, _T<U,S,E,R>, sizeof(data), data);

		printf("USER: %s", data);

		return 1;
	}

	return 0;
}
