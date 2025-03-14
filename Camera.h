#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
public:

	Camera();
	~Camera();

	void SetPosition(float x, float y, float z);

	void SetLens(float fovY, float aspect, float zn, float zf);

	XMFLOAT3 GetPosition()const;
	XMMATRIX GetView()const;
	XMMATRIX GetProj()const;
	XMFLOAT4X4 GetProj4x4f()const;
	XMFLOAT4X4 GetPixelized()const;

	void Strafe(float d);
	void Walk(float d);

	void Pitch(float angle);
	void RotateY(float angle);

	void UpdateViewMatrix();
	void UpdatePixelizedMatrix();
 

private:

	XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	XMFLOAT4X4 mView =
	{
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f,
	};
	XMFLOAT4X4 mProj =
	{
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f,
	};

	XMFLOAT4X4 mPixelized =
	{
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f,
	};


};

