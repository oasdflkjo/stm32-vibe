#include "fault/fault.h"
#include "trace/trace.h"

void fault_report_emit(const fault_report_t *report)
{
    (void)report;
    trace_abort_record();
    TRACE("FAULT type=%u pc=%08X lr=%08X",
          report->type, report->pc, report->lr);
    TRACE("FAULT cfsr=%08X hfsr=%08X", report->cfsr, report->hfsr);
    TRACE("FAULT mmfar=%08X bfar=%08X xpsr=%08X",
          report->mmfar, report->bfar, report->xpsr);
}
