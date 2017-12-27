#include <YSI\y_hooks>


hook OnScriptInit()
{
	new data[2];

	modio_read(FILE_STORAGE, _T<M,O,D,3>, sizeof(data), data);

	printf("Module 3 last run at %d with a tickcount of %d", data[0], data[1]);
}

hook OnScriptExit()
{
	new data[2];
	data[0] = gettime();
	data[1] = GetTickCount();
	modio_push(FILE_STORAGE, _T<M,O,D,3>, sizeof(data), data);
}


hook OnRconCommand(cmd[])
{
	if(!strcmp(cmd, "save", false, 4))
	{
		new data[1];

		data[0] = heapspace();

		modio_push(FILE_RUNTIME, _T<H,E,A,P>, 1, data);

		return 1;
	}

	if(!strcmp(cmd, "load", false, 4))
	{
		new data[1];

		modio_read(FILE_RUNTIME, _T<H,E,A,P>, sizeof(data), data);

		printf("HEAP: %d", data);

		return 1;
	}

	return 0;
}
