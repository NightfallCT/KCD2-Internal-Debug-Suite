#pragma once
#include "Windows.h"
#include <iostream>
#include "../hooks/ig-hooks.h"
#include <cmath>
#define _USE_MATH_DEFINES


namespace Fly {
	inline bool flyEnabled = false;
	inline float flySpeed = 10.0f;
	inline bool bBoostEnabled = false;

	extern void flyLoop();
}
