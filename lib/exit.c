
#include <inc/lib.h>

void
exit(void)
{
	// NOTE: 临时注释掉下面这行
	// close_all();
	sys_env_destroy(0);
}

