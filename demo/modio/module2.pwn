#include <YSI\y_hooks>


hook OnFilterScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, !"MOD2", data);

	printf("Module 2 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnFilterScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, !"MOD2", sizeof(data), data);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[2];

		data[0] = gettime();
		data[1] = GetTickCount();

		modio_push(FILE_RUNTIME, !"TIME", 2, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[2];

		modio_read(FILE_RUNTIME, !"TIME", data);

		printf("TIME: %d, %d", data[0], data[1]);

		return 1;
	}

	return 0;
}
