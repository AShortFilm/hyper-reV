#pragma once
#include "../structures/trap_frame.h"

namespace hypercall
{
	void process(trap_frame_t* trap_frame);
}
