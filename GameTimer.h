#pragma once
#include<windows.h>
class GameTimer
{
public:
	GameTimer();

	float TotalTime() const;//用秒做单位
	float DeltaTime() const;//用秒做单位

	void Reset();//在开始消息循环之前用
	void Start();//解除暂停
	void Stop();//暂时计数器时使用
	void Tick();//每帧调用的逻辑

private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped = true;
};


