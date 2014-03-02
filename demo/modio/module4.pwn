#include <YSI\y_hooks>


hook OnFilterScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, !"MOD4", data);

	printf("Module 4 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnFilterScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, !"MOD4", sizeof(data), data, .forcewrite = true);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[10];

		data = "Southclaw";

		modio_push(FILE_RUNTIME, !"USER", 10, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[10];

		modio_read(FILE_RUNTIME, !"USER", data);

		printf("USER: %s", data);

		return 1;
	}

	return 0;
}
