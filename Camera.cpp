#include "Camera.h"

Camera::Camera()
{
	SetLens(0.25f * 3.1415926f, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
	mViewDirty = true;
}


void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	//构建投影矩阵（自动进行了透视除法，放在里面了）因为W是负责透视和深度信息保存的（乘以逆矩阵后，他恢复就是坐标（相对）本身了，而不是具体的数值）
	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}


XMMATRIX Camera::GetView()const
{
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj()const
{
	return XMLoadFloat4x4(&mProj);
}

XMFLOAT4X4 Camera::GetProj4x4f()const
{
	return mProj;
}
XMFLOAT4X4 Camera::GetPixelized()const
{
	return mPixelized;
}
XMFLOAT3 Camera::GetPosition()const
{
	return mPosition;
}
void Camera::Strafe(float d)
{
	// mPosition += d*mRight
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

	mViewDirty = true;
}

void Camera::Walk(float d)
{
	// mPosition += d*mLook
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

	mViewDirty = true;
}

void Camera::Pitch(float angle)
{

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::RotateY(float angle)
{

	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}



void Camera::UpdateViewMatrix()
{
	if (mViewDirty)
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		R = XMVector3Cross(U, L);


		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		/* a 代表相机的右向量（Right Vector），通常表示为 a = (a_x, a_y, a_z)。
		   b 代表相机的上向量（Up Vector），通常表示为 b = (b_x, b_y, b_z)。
		   c 代表相机的前向量（Forward Vector|Look Vector），通常表示为 c = (c_x, c_y, c_z)。 */
		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView(0, 0) = mRight.x;
		mView(1, 0) = mRight.y;
		mView(2, 0) = mRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mUp.x;
		mView(1, 1) = mUp.y;
		mView(2, 1) = mUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mLook.x;
		mView(1, 2) = mLook.y;
		mView(2, 2) = mLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
	}
}
//更新像素矩阵
void Camera::UpdatePixelizedMatrix()
{
	XMVECTOR B = XMLoadFloat3(&mLook);

	B = XMVectorNegate(B);

	XMFLOAT3 mBack;
	XMStoreFloat3(&mBack, B);

	mPixelized(0, 0) = mRight.x;
	mPixelized(1, 0) = mRight.y;
	mPixelized(2, 0) = mRight.z;
	mPixelized(3, 0) = 0;

	mPixelized(0, 1) = mUp.x;
	mPixelized(1, 1) = mUp.y;
	mPixelized(2, 1) = mUp.z;
	mPixelized(3, 1) = 0;

	mPixelized(0, 2) = mBack.x;
	mPixelized(1, 2) = mBack.y;
	mPixelized(2, 2) = mBack.z;
	mPixelized(3, 2) = 0;

	mPixelized(0, 3) = 0.0f;
	mPixelized(1, 3) = 0.0f;
	mPixelized(2, 3) = 0.0f;
	mPixelized(3, 3) = 1.0f;

}
