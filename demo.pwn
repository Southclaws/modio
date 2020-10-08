#include <a_samp>
#include <logger>

#include "modio.inc"

#define FILE_STORAGE "storage.dat"
#define FILE_RUNTIME "runtime.dat"

#include "demo/module1.pwn"
#include "demo/module2.pwn"
#include "demo/module3.pwn"
#include "demo/module4.pwn"

main() {
	Logger_Log("Demo initialising");
	return 1;
}
