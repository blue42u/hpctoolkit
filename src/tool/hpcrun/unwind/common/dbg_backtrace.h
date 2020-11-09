//
// Debug interface to backtrace, primarily for simulating
// exceptional conditions
//
#ifndef DBG_BACKTRACE_H
#define DBG_BACKTRACE_H

__attribute__((visibility("default")))
void (*hpcrun_dbg_unw_init_cursor)(hpcrun_unw_cursor_t* cursor, ucontext_t* context);
__attribute__((visibility("default")))
bool (*hpcrun_dbg_trampoline_interior)(void* ip);
__attribute__((visibility("default")))
bool (*hpcrun_dbg_trampoline_at_entry)(void* ip);
__attribute__((visibility("default")))
step_state (*hpcrun_dbg_unw_step)(hpcrun_unw_cursor_t* cursor);


#endif // DBG_BACKTRACE_H
