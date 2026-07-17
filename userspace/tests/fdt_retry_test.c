#include "gxfp/flow/fdt.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

int main(void)
{
	struct gxfp_fdt_flow flow;

	assert(gxfp_fdt_flow_wait_up_retry_due(NULL, 1000) == -EINVAL);

	memset(&flow, 0, sizeof(flow));
	flow.mode = GXFP_FDT_MODE_WAIT_UP;
	flow.state = GXFP_FDT_STATE_DOWN;
	flow.wait_up_armed_ms = 1000;

	assert(gxfp_fdt_flow_wait_up_retry_due(&flow, 999) == 0);
	assert(gxfp_fdt_flow_wait_up_retry_due(&flow, 1749) == 0);
	assert(gxfp_fdt_flow_wait_up_retry_due(&flow, 1750) == 1);

	flow.state = GXFP_FDT_STATE_UP;
	assert(gxfp_fdt_flow_wait_up_retry_due(&flow, 5000) == 0);

	flow.state = GXFP_FDT_STATE_DOWN;
	flow.wait_up_retries = 3;
	assert(gxfp_fdt_flow_wait_up_retry_due(&flow, 1750) == -ETIMEDOUT);

	return 0;
}
