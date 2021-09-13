#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

long (*STUB_start_elevator)(void) = NULL;
EXPORT_SYMBOL(STUB_start_elevator);
SYSCALL_DEFINE0(start_elevator) {
	if(STUB_start_elevator != NULL)
		return STUB_start_elevator();
	else
		return -ENOSYS;
}

long (*STUB_issue_request)(int, int, int) = NULL;
EXPORT_SYMBOL(STUB_issue_request);
SYSCALL_DEFINE3(issue_request, int, passenger_type, int, start_floor, int, dest_floor) {
	if(STUB_issue_request != NULL)
		return STUB_issue_request(passenger_type, start_floor, dest_floor);
	else
		return -ENOSYS;
}

long(*STUB_stop_elevator)(void) = NULL;
EXPORT_SYMBOL(STUB_stop_elevator);
SYSCALL_DEFINE0(stop_elevator) {
	if(STUB_stop_elevator != NULL)
		return STUB_stop_elevator();
	else
		return -ENOSYS;
}
