#include "components/CarouselComponent.h"
#include <iostream>
#include <boost/filesystem.hpp>
#include <math.h>
#include "Log.h"
#include "Renderer.h"
#include "ThemeData.h"
#include "Util.h"



CarouselComponent::CarouselComponent(Window* window, bool forceLoad, bool dynamic) : GuiComponent(window), IList<type, type>
	// initialisationList here
{
	
	setDefaults();
}

CarouselComponent::~CarouselComponent()
{
	// destructor code, if needed.
}

void CarouselComponent::setDefaults()
{
	// Carousel legacy values
	mType = HORIZONTAL;
	mCarouselSize.x() = mSize.x();
	mCarouselSize.y() = 0.2325f * mSize.y();
	mPos.x() = 0.0f;
	mPos.y() = 0.5f * (mSize.y() - mCarouselSize.y());
	mColor = 0xFFFFFFD8;
	mLogoScale = 1.2f;
	mLogoSize.x() = 0.25f * mSize.x();
	mLogoSize.y() = 0.155f * mSize.y();
	mMaxLogoCount = 3;
	mZIndex = 40;
}

void CarouselComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);  // takes care of pos, size, zindex, 

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "carousel");
	if (!elem)
		return;

	using namespace ThemeFlags;

	//if ((properties & ORIGIN || (properties & POSITION && properties & ThemeFlags::SIZE)) && elem->has("origin"))
	//	setOrigin(elem->get<Eigen::Vector2f>("origin"));

	if (properties & TEXT && elem->has("type"))
		mType = (elem->get<std::string>("type") == "vertical") ? VERTICAL : HORIZONTAL;
	if (properties & COLOR && elem->has("color"))
		mColor = elem->get<unsigned int>("color");
	if (elem->has("logoScale"))
		mLogoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize")) {
		Eigen::Vector2f scale = getParent() ? getParent()->getSize() : Eigen::Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		mLogoSize = elem->get<Eigen::Vector2f>("logoSize").cwiseProduct(scale);
	}
		
	if (elem->has("maxLogoCount"))
		mMaxLogoCount = std::round(elem->get<float>("maxLogoCount"));
}

void CarouselComponent::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = getTransform() * parentTrans;

	Eigen::Vector2i clipPos((int)mPos.x(), (int)mPos.y());
	Eigen::Vector2i clipSize((int)mSize.x(), (int)mSize.y());

	Renderer::pushClipRect(clipPos, clipSize);

	// background box behind logos
	Renderer::setMatrix(trans);
	Renderer::drawRect(mPos.x(), mPos.y(), mSize.x(), mSize.y(), mColor);

	// draw logos
	Eigen::Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mType)
	{
	case VERTICAL:
		logoSpacing[1] = ((mSize.y() - (mLogoSize.y() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.y();
		xOff = mPos.x() + (mSize.x() / 2) - (mLogoSize.x() / 2);
		yOff = mPos.y() + (mSize.y() - mLogoSize.y()) / 2 - (mCamOffset * logoSpacing[1]);
		break;
	case HORIZONTAL:
	default:
		logoSpacing[0] = ((mSize.x() - (mLogoSize.x() * mMaxLogoCount)) / (mMaxLogoCount)) + mLogoSize.x();
		xOff = mPos.x() + (mSize.x() - mLogoSize.x()) / 2 - (mCamOffset * logoSpacing[0]);
		yOff = mPos.y() + (mSize.y() / 2) - (mLogoSize.y() / 2);
		break;
	}

	Eigen::Affine3f logoTrans = trans;
	int center = (int)(mCamOffset);
	int logoCount = std::min(mMaxLogoCount, (int)mEntries.size());

	// Adding texture loading buffers depending on scrolling speed and status
	int bufferIndex = getScrollingVelocity() + 1;

	for (int i = center - logoCount / 2 + logoBuffersLeft[bufferIndex]; i <= center + logoCount / 2 + logoBuffersRight[bufferIndex]; i++)
	{
		int index = i;
		while (index < 0)
			index += mEntries.size();
		while (index >= (int)mEntries.size())
			index -= mEntries.size();

		logoTrans.translation() = trans.translation() + Eigen::Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0);

		if (index == mCursor) //Selected System
		{
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logoSelected;
			comp->setOpacity(0xFF);
			comp->render(logoTrans);
		}
		else { // not selected systems
			const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logo;
			comp->setOpacity(0x80);
			comp->render(logoTrans);
		}
	}
	Renderer::popClipRect();

	GuiComponent::renderChildren(trans);
}

// Returns the center point of the Carousel (takes origin into account).
Eigen::Vector2f CarouselComponent::getCenter() const
{
	return Eigen::Vector2f(mPosition.x() - (getSize().x() * mOrigin.x()) + getSize().x() / 2, 
		mPosition.y() - (getSize().y() * mOrigin.y()) + getSize().y() / 2);
}

void CarouselComponent::setOrigin(float originX, float originY)
{
	//mOrigin << originX, originY;
	//updateVertices();
}


std::vector<HelpPrompt> CarouselComponent::getHelpPrompts()
{
	//std::vector<HelpPrompt> ret;
	//ret.push_back(HelpPrompt("a", "select"));
	//return ret;

	std::vector<HelpPrompt> prompts;
	if (mType == VERTICAL)
		prompts.push_back(HelpPrompt("up/down", "choose"));
	else
		prompts.push_back(HelpPrompt("left/right", "choose"));
	prompts.push_back(HelpPrompt("a", "select"));
	prompts.push_back(HelpPrompt("x", "random"));
	return prompts;
}
