#include "GameTimer.h"
GameTimer::GameTimer() : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0)
{
	__int64 countsPersec;
	QueryPerformanceCounter((LARGE_INTEGER*)&countsPersec);
	mSecondsPerCount = 1.0 / (double)countsPersec;

}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);
	mCurrTime = currentTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = mCurrTime;

	//如果处理器在执行时切换了，可能为负值（两次执行QueryPerformance的位置不同）
	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}

float GameTimer::DeltaTime() const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currentTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);

	mBaseTime = currentTime;
	mPrevTime = currentTime;
	mDeltaTime = 0.0;
	mStopped = false;
}

void GameTimer::Stop()
{
	//停止了就什么也不做
	if (!mStopped)
	{
		__int64 currentTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&currentTime);

		mStopTime = currentTime;
		mStopped = true;
	}
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

	// --------*------------*---------
	//	  mStopTime		startTime
	//mPausedTime 就从中间算

	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		//重置前一帧的时间
		mPrevTime = startTime;

		mStopTime = 0;
		mStopped = false;
	}
}

float GameTimer::TotalTime()const
{
	//		|<-------------->|<----------->|					  |<----------------->|
	// ---*-----------------*----------------*---------------*----------------*-------------------*------------------->
	//  mBase Time		mStopTime0		  starttIME		mCurrentTime	  mStopTime			   mCurrentTime
	//两种情况，在上面标出来了
	if (mStopped)
	{
		return (float)((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount;
	}

	else
	{
		return (float)((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount;
	}
}