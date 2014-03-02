#include <YSI\y_hooks>


hook OnFilterScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, !"MOD3", data);

	printf("Module 3 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnFilterScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, !"MOD3", sizeof(data), data);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[1];

		data[0] = heapspace();

		modio_push(FILE_RUNTIME, !"HEAP", 1, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[1];

		modio_read(FILE_RUNTIME, !"HEAP", data);

		printf("HEAP: %d", data);

		return 1;
	}

	return 0;
}
