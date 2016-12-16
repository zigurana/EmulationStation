#include "views/SystemView.h"
#include "SystemData.h"
#include "Renderer.h"
#include "Log.h"
#include "Window.h"
#include "views/ViewController.h"
#include "animations/LambdaAnimation.h"
#include "SystemData.h"
#include "Settings.h"
#include "Util.h"

SystemView::SystemView(Window* window) : IList<SystemViewData, SystemData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP),
	mSystemInfo(window, "SYSTEM INFO", Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER)
{
	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	populate();
}

void SystemView::populate()
{
	mEntries.clear();

	for(auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
	{
		const std::shared_ptr<ThemeData>& theme = (*it)->getTheme();

		onThemeChanged(theme);

		Entry e;
		e.name = (*it)->getName();
		e.object = *it;

		// make logo
		if(theme->getElement("system", "logo", "image"))
		{
			ImageComponent* logo = new ImageComponent(mWindow);
			logo->setMaxSize(Eigen::Vector2f(mCarousel.logoSizeX, mCarousel.logoSizeY));
			logo->applyTheme((*it)->getTheme(), "system", "logo", ThemeFlags::PATH);
			logo->setPosition((mCarousel.logoSizeX - logo->getSize().x()) / 2,
				              (mCarousel.logoSizeY - logo->getSize().y()) / 2); // center
			e.data.logo = std::shared_ptr<GuiComponent>(logo);

			ImageComponent* logoSelected = new ImageComponent(mWindow);
			logoSelected->setMaxSize(Eigen::Vector2f(mCarousel.logoSizeX * mCarousel.logoScale, mCarousel.logoSizeY * mCarousel.logoScale));
			logoSelected->applyTheme((*it)->getTheme(), "system", "logo", ThemeFlags::PATH);
			logoSelected->setPosition((mCarousel.logoSizeX - logoSelected->getSize().x()) / 2,
									  (mCarousel.logoSizeY - logoSelected->getSize().y()) / 2); // center
			e.data.logoSelected = std::shared_ptr<GuiComponent>(logoSelected);
		}else{
			// no logo in theme; use text
			TextComponent* text = new TextComponent(mWindow, 
				(*it)->getName(), 
				Font::get(FONT_SIZE_LARGE), 
				0x000000FF, 
				ALIGN_CENTER);
			text->setSize(logoSize());
			e.data.logo = std::shared_ptr<GuiComponent>(text);

			TextComponent* textSelected = new TextComponent(mWindow, 
				(*it)->getName(), 
					//Font::get((int)(FONT_SIZE_LARGE * SELECTED_SCALE)),
					Font::get((int)(FONT_SIZE_LARGE * 1.5)),
				0x000000FF, 
				ALIGN_CENTER);
			textSelected->setSize(logoSize());
			e.data.logoSelected = std::shared_ptr<GuiComponent>(textSelected);
		}

		// make background extras
		e.data.backgroundExtras = std::shared_ptr<ThemeExtras>(new ThemeExtras(mWindow));
		e.data.backgroundExtras->setExtras(ThemeData::makeExtras((*it)->getTheme(), "system", mWindow));

		this->add(e);
	}
}

void SystemView::goToSystem(SystemData* system, bool animate)
{
	setCursor(system);

	if(!animate)
		finishAnimation(0);
}

bool SystemView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_r && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading all";
			ViewController::get()->reloadAll();
			//// reload themes
			//for(auto it = mEntries.begin(); it != mEntries.end(); it++)
			//	it->object->loadTheme();

			//populate();
			//updateHelpPrompts();
			return true;
		}
		if(config->isMappedTo("left", input))
		{
			listInput(-1);
			return true;
		}
		if(config->isMappedTo("right", input))
		{
			listInput(1);
			return true;
		}
		if(config->isMappedTo("a", input))
		{
			stopScrolling();
			ViewController::get()->goToGameList(getSelected());
			return true;
		}
	}else{
		if(config->isMappedTo("left", input) || config->isMappedTo("right", input))
			listInput(0);
	}

	return GuiComponent::input(config, input);
}

void SystemView::update(int deltaTime)
{
	listUpdate(deltaTime);
	GuiComponent::update(deltaTime);
}

void SystemView::onCursorChanged(const CursorState& state)
{
	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = abs(endPos - startPos);
	
	if(abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if(abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)

	
	// animate mSystemInfo's opacity (fade out, wait, fade back in)

	cancelAnimation(1);
	cancelAnimation(2);

	const float infoStartOpacity = mSystemInfo.getOpacity() / 255.f;

	Animation* infoFadeOut = new LambdaAnimation(
		[infoStartOpacity, this] (float t)
	{
		mSystemInfo.setOpacity((unsigned char)(lerp<float>(infoStartOpacity, 0.f, t) * 255));
	}, (int)(infoStartOpacity * 150));

	unsigned int gameCount = getSelected()->getGameCount();

	// also change the text after we've fully faded out
	setAnimation(infoFadeOut, 0, [this, gameCount] {
		std::stringstream ss;
		
		if (getSelected()->getName() == "retropie")
			ss << "CONFIGURATION";
		// only display a game count if there are at least 2 games
		else if(gameCount > 1)
			ss << gameCount << " GAMES AVAILABLE";

		mSystemInfo.setText(ss.str()); 
	}, false, 1);

	// only display a game count if there are at least 2 games
	if(gameCount > 1)
	{
		Animation* infoFadeIn = new LambdaAnimation(
			[this](float t)
		{
			mSystemInfo.setOpacity((unsigned char)(lerp<float>(0.f, 1.f, t) * 255));
		}, 300);

		// wait 600ms to fade in
		setAnimation(infoFadeIn, 2000, nullptr, false, 2);
	}

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	if(endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	Animation* anim;
	if(Settings::getInstance()->getString("TransitionStyle") == "fade")
	{
		float startExtrasFade = mExtrasFadeOpacity;
		anim = new LambdaAnimation(
			[startExtrasFade, startPos, endPos, posMax, this](float t)
		{
			t -= 1;
			float f = lerp<float>(startPos, endPos, t*t*t + 1);
			if(f < 0)
				f += posMax;
			if(f >= posMax)
				f -= posMax;

			this->mCamOffset = f;

			t += 1;
			if(t < 0.3f)
				this->mExtrasFadeOpacity = lerp<float>(0.0f, 1.0f, t / 0.3f + startExtrasFade);
			else if(t < 0.7f)
				this->mExtrasFadeOpacity = 1.0f;
			else
				this->mExtrasFadeOpacity = lerp<float>(1.0f, 0.0f, (t - 0.7f) / 0.3f);

			if(t > 0.5f)
				this->mExtrasCamOffset = endPos;

		}, 500);
	}
	else{ // slide
		anim = new LambdaAnimation(
			[startPos, endPos, posMax, this](float t)
		{
			t -= 1;
			float f = lerp<float>(startPos, endPos, t*t*t + 1);
			if(f < 0)
				f += posMax;
			if(f >= posMax)
				f -= posMax;

			this->mCamOffset = f;
			this->mExtrasCamOffset = f;
		}, 500);
	}

	setAnimation(anim, 0, nullptr, false, 0);
}

void SystemView::render(const Eigen::Affine3f& parentTrans)
{
	if(size() == 0)
		return;  // nothing to render
	
	renderExtras(parentTrans);
	renderCarousel(parentTrans);
}

std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("left/right", "choose"));
	prompts.push_back(HelpPrompt("a", "select"));
	return prompts;
}

HelpStyle SystemView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mEntries.at(mCursor).object->getTheme(), "system");
	return style;
}


// It would be cleanest if this function handles all theme-related actions,
// but for now, it just sets the struct for the carousel parameters.
void  SystemView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	LOG(LogDebug) << "SystemView::onThemeChanged()";

	// set up defaults
	mCarousel.height          = 0.2f * mSize.y();
	mCarousel.ypos            = 0.5f * (mSize.y() - mCarousel.height); // default is centered
	mCarousel.color           = 0xFFFFFFFF; 
	mCarousel.infoBarColor    = 0xDDDDDDFF;		
	mCarousel.logoScale       = 1.5f;
	mCarousel.logoSizeX       = 0.25f * mSize.y();
	mCarousel.logoSizeY		  = 0.155f * mSize.y();
	mCarousel.maxLogoCount    = 3;
	std::string  fpath        = Font::getDefaultPath();
	float        fsize        = 0.035f;
	unsigned int fcolor       = 0x000000ff;

	const ThemeData::ThemeElement* elem = theme->getElement("system", "carousel", "systemcarousel");
	if(elem)
	{
		if (elem->has("height"))
			mCarousel.height = elem->get<float>("height") * mSize.y();
		if (elem->has("ypos"))
			mCarousel.ypos = elem->get<float>("ypos") * mSize.y();
		if (elem->has("color"))
			mCarousel.color = elem->get<unsigned int>("color");
		if (elem->has("infobarcolor"))
			mCarousel.infoBarColor = elem->get<unsigned int>("infobarcolor");
		if (elem->has("logoscale"))
			mCarousel.logoScale = elem->get<float>("logoscale");
		if (elem->has("logosizex"))
			mCarousel.logoSizeX = elem->get<float>("logosizex") * mSize.y();
		if (elem->has("logosizey"))
			mCarousel.logoSizeY = elem->get<float>("logosizey") * mSize.y();
		if (elem->has("maxlogocount"))
			mCarousel.maxLogoCount = std::round(elem->get<float>("maxlogocount"));
		if (elem->has("infobarfontpath"))
			fpath = elem->get<std::string>("infobarfontpath");
		if (elem->has("infobarfontsize"))
			fsize = elem->get<float>("infobarfontsize");
		if (elem->has("infobarfontcolor"))
			fcolor = elem->get<unsigned int>("infobarfontcolor");
	}
	mCarousel.logoSpacingX = (mSize.x() - (mCarousel.logoSizeX * mCarousel.maxLogoCount)) / (mCarousel.maxLogoCount);
	mSystemInfo.setFont(Font::get((int)(fsize * mSize.y()), fpath));
	mSystemInfo.setColor(fcolor);
	mSystemInfo.setSize(mSize.x(), mSystemInfo.getFont()->getLetterHeight()*2.2f);
	mSystemInfo.setPosition(0, (mCarousel.ypos + mCarousel.height));
	}

// Render system carousel
void SystemView::renderCarousel(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = getTransform() * parentTrans;

	// Get number of logo's in caroussel
	int logoCount = std::min(mCarousel.maxLogoCount, (int)mEntries.size());

	// background behind the logos <- Caroussel
	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, mCarousel.ypos, mSize.x(), mCarousel.height, mCarousel.color);

	// draw logos
	float xOff = (mSize.x() - mCarousel.logoSizeX) / 2 - (mCamOffset * (mCarousel.logoSizeX + mCarousel.logoSpacingX));
	float yOff = mCarousel.ypos + (mCarousel.height / 2) - (mCarousel.logoSizeY / 2);
	Eigen::Affine3f logoTrans = trans;

	int center = (int)(mCamOffset);
	for (int i = center - logoCount / 2; i < center + logoCount / 2 + 1; i++)
	{
		int index = i;
		while (index < 0)
			index += mEntries.size();
		while (index >= (int)mEntries.size())
			index -= mEntries.size();

		logoTrans.translation() = trans.translation() + Eigen::Vector3f(i * (mCarousel.logoSizeX + mCarousel.logoSpacingX) + xOff, yOff, 0);

		if (index == mCursor) //scale our selection up
		{
			// selected
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logoSelected;
			comp->setOpacity(0xFF);
			comp->render(logoTrans);
		}
		else {
			// not selected
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logo;
			comp->setOpacity(0x80);
			comp->render(logoTrans);
		}
	}

	// Finally, render systeminfo bar and text
	Renderer::setMatrix(trans);
	Renderer::drawRect(mSystemInfo.getPosition().x(), mSystemInfo.getPosition().y() - 1,
					mSize.x(), mSystemInfo.getSize().y(),
					mCarousel.infoBarColor | (unsigned char)(mSystemInfo.getOpacity() / 255.f));
	mSystemInfo.render(trans);
}

// Draw background extras
void SystemView::renderExtras(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = getTransform() * parentTrans;
	Eigen::Affine3f extrasTrans = trans;
	int extrasCenter = (int)mExtrasCamOffset;
	for (int i = extrasCenter - 1; i < extrasCenter + 2; i++)
	{
		int index = i;
		while (index < 0)
			index += mEntries.size();
		while (index >= (int)mEntries.size())
			index -= mEntries.size();

		extrasTrans.translation() = trans.translation() + Eigen::Vector3f((i - mExtrasCamOffset) * mSize.x(), 0, 0);

		Eigen::Vector2i clipRect = Eigen::Vector2i((int)((i - mExtrasCamOffset) * mSize.x()), 0);
		Renderer::pushClipRect(clipRect, mSize.cast<int>());
		mEntries.at(index).data.backgroundExtras->render(extrasTrans);
		Renderer::popClipRect();
	}

	// fade extras if necessary
	if (mExtrasFadeOpacity)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0x00000000 | (unsigned char)(mExtrasFadeOpacity * 255));
	}
}