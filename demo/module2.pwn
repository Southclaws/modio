#include <YSI\y_hooks>


hook OnScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, _T<M,O,D,2>, sizeof(data), data);

	printf("Module 2 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, _T<M,O,D,2>, sizeof(data), data);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[2];

		data[0] = gettime();
		data[1] = GetTickCount();

		modio_push(FILE_RUNTIME, _T<T,I,M,E>, 2, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[2];

		modio_read(FILE_RUNTIME, _T<T,I,M,E>, sizeof(data), data);

		printf("TIME: %d, %d", data[0], data[1]);

		return 1;
	}

	return 0;
}
