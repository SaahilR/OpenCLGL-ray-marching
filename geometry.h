#pragma once
#include "linear_algebra.h"

struct Object
{
	float radius;
	float type;
	float reflectance;
	float geometry_type;
	Vector3Df position;
	Vector3Df color;
	Vector3Df emission;
};
