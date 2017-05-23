#include "components/ScrollableTextComponent.h"

ScrollableTextComponent::ScrollableTextComponent(Window* window) : GuiComponent(window), TextComponent(window), ScrollableContainer(window),
	mText(window), mContainer(window)
{
	mContainer.addChild(&mText);
	mContainer.setAutoScroll(true);
}

void ScrollableTextComponent::render(const Eigen::Affine3f& parentTrans)
{
	//mText.render(parentTrans);
	mContainer.render(parentTrans);
}

void ScrollableTextComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	using namespace ThemeFlags;
	mContainer.applyTheme(theme, view, element, properties); 
	mText.setSize(mContainer.getSize().x(), 0);
	mText.applyTheme(theme, view, element, ALL ^ (POSITION | ThemeFlags::SIZE | TEXT));
}

void ScrollableTextComponent::setOpacity(unsigned char opacity)
{
	mText.setOpacity(opacity);
	mContainer.setOpacity(opacity);
}

unsigned char ScrollableTextComponent::getOpacity() const
{
	// opacity of both mText and mContainer are the same, return mContainer
	return mContainer.getOpacity();
}

void ScrollableTextComponent::update(int deltaTime)
{
	mText.update(deltaTime);
	mContainer.update(deltaTime);
}

void ScrollableTextComponent::onSizeChanged()
{
	mText.onSizeChanged();
	mContainer.onSizeChanged();
}

std::string ScrollableTextComponent::getValue() const
{
	return mText.getValue();
}

void ScrollableTextComponent::setValue(const std::string& value)
{
	mText.setValue(value);
	mContainer.reset();
}
