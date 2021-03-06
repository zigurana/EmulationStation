#pragma once

#include "Window.h"

class VideoComponent;
class ImageComponent;
class Sound;

// Screensaver implementation for main window
class SystemScreenSaver : public Window::ScreenSaver
{
public:
	SystemScreenSaver(Window* window);
	virtual ~SystemScreenSaver();

	virtual void startScreenSaver();
	virtual void stopScreenSaver();
	virtual void nextVideo();
	virtual void renderScreenSaver();
	virtual bool allowSleep();
	virtual void update(int deltaTime);
	virtual bool isScreenSaverActive();

	virtual FileData* getCurrentGame();
	virtual void launchGame();

private:
	unsigned long countGameListNodes(const char *nodeName);
	void countVideos();
	void countImages();
	void pickGameListNode(unsigned long index, const char *nodeName, std::string& path);
	void pickRandomVideo(std::string& path);
	void pickRandomGameListImage(std::string& path);
	void pickRandomCustomImage(std::string& path);

	void input(InputConfig* config, Input input);

	enum STATE {
		STATE_INACTIVE,
		STATE_FADE_OUT_WINDOW,
		STATE_FADE_IN_VIDEO,
		STATE_SCREENSAVER_ACTIVE
	};

private:
	bool			mVideosCounted;
	unsigned long		mVideoCount;
	VideoComponent*		mVideoScreensaver;
	bool			mImagesCounted;
	unsigned long		mImageCount;
	ImageComponent*		mImageScreensaver;
	Window*			mWindow;
	STATE			mState;
	float			mOpacity;
	int				mTimer;
	FileData*		mCurrentGame;
	std::string		mGameName;
	std::string		mSystemName;
	int 			mVideoChangeTime;
	std::shared_ptr<Sound>	mBackgroundAudio;
	bool			mStopBackgroundAudio;
};
