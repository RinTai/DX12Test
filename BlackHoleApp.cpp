#include "BlackHoleApp.h"

class BlackHoleApp : public DxApp
{
public:
    BlackHoleApp(HINSTANCE hInstance);
    BlackHoleApp(const BlackHoleApp& rhs) = delete;
    BlackHoleApp& operator=(const BlackHoleApp& rhs) = delete;
    ~BlackHoleApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;


    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

};