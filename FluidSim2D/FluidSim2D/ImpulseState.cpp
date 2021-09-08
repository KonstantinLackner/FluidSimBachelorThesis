#include "ImpulseState.h"

ImpulseState::ImpulseState()
    : ForceActive(false)
    , InkActive(false)
    , Radial(false)
    , LastPos()
    , CurrentPos()
    , Delta()
    , RainbowModeHue()
{
}

void ImpulseState::Update(float x, float y, bool left_down, bool right_down)
{
    bool down = left_down || right_down;

    if (!IsActive() && down)
    {
        CurrentPos.x = x;
        CurrentPos.y = y;
        ForceActive = true;
        InkActive = left_down && !right_down;
    }
    else if (IsActive() && down)
    {
        auto temp = CurrentPos;
        CurrentPos.x = x;
        CurrentPos.y = y;
        LastPos = temp;
    }
    else if (IsActive() && !down)
    {
        LastPos.x = LastPos.y = 0.f;
        Reset();
    }

    Delta = CurrentPos - LastPos;
}

void ImpulseState::Reset()
{
    static glm::vec3 zero(0, 0, 0);
    Delta = zero;
    CurrentPos = zero;
    LastPos = zero;
    ForceActive = false;
    InkActive = false;
    Radial = false;
}

float HueToRGB(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < (1.f / 6.f)) return p + (q - p) * 6 * t;
    if (t < (1.f / 2.f)) return q;
    if (t < (2.f / 3.f)) return p + (q - p) * ((2.f / 3.f) - t) * 6;
    return p;
};

// Credit to mjackson: https://gist.github.com/mjackson/5311256
glm::vec4 HSLToRGB(float h, float s, float l)
{
    float r, g, b;

    if (s == 0) 
    {
        r = g = b = l; // achromatic
    }
    else 
    {
        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;
        r = HueToRGB(p, q, h + (1.f / 3.f));
        g = HueToRGB(p, q, h);
        b = HueToRGB(p, q, h - (1.f / 3.f));
    }

    return glm::vec4(r, g, b, 1.0f);
}

glm::vec4 ImpulseState::TickRainbowMode(float delta_t)
{
	static constexpr auto RainbowModeHueMultiplier = 1.0f;

    RainbowModeHue += (delta_t / 0.016667f) * RainbowModeHueMultiplier;
    if (RainbowModeHue > 360.f)
        RainbowModeHue = 0;

    return HSLToRGB(RainbowModeHue / 360, 1, 0.5);
}