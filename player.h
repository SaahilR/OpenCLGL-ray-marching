#pragma once

#include "linear_algebra.h"

class Player {
	Vector3Df position;
	Vector3Df velocity;
	Vector3Df look_dir;

public:
	Player();
	~Player();
	void addPosition(Vector3Df p);
	void addVelocity(Vector3Df v);
	void setPosition(Vector3Df p);
	void setVelocity(Vector3Df v);
	void setLookDir(Vector3Df l);

	void Jump();
	void updatePlayer(Vector3Df p);
};