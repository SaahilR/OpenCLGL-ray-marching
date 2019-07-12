#include "player.h"

Player::Player() {
	position = Vector3Df(0.0f, 0.0f, 0.0f);
	velocity = Vector3Df(0.0f, 0.0f, 0.0f);
}

Player::~Player() {}

void Player::setPosition(Vector3Df p) {
	position = p;
}
void Player::setVelocity(Vector3Df v) {
	velocity = v;
}

void Player::setLookDir(Vector3Df l) {
	look_dir = l;
}

void Player::addPosition(Vector3Df p) {
	position += p;
}
void Player::addVelocity(Vector3Df v) {
	velocity += v;
}

void Player::Jump() {}


void Player::updatePlayer(Vector3Df p) {
	setPosition(p + look_dir * velocity);
	setVelocity(velocity * 0.5f);
}