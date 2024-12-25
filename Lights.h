#pragma once
#include "d3dUtil.h"
//作为光源类和阴影类
enum LightType
{
	Direction,
	Spot,
	Point,
};
class Lights
{
public:
private:
	LightType Type;
	XMFLOAT3 Position;
	XMFLOAT3 Direction;
};

