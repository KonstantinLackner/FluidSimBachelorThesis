#pragma once

#include <glm/glm.hpp>

struct ImpulseState
{
	ImpulseState();

	void Update(float x, float y, bool left_down, bool right_down);
	void Reset();
	bool IsActive() const { return InkActive || ForceActive; }
	glm::vec4 TickRainbowMode(float delta_t);

	glm::vec3 LastPos;
	glm::vec3 CurrentPos;
	bool ForceActive;
	bool InkActive;
	bool Radial;
	glm::vec3 Delta;

	float RainbowModeHue;
};