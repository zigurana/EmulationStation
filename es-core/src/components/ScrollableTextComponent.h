#ifndef _SCROLLABLETEXTCOMPONENT_H_
#define _SCROLLABLETEXTCOMPONENT_H_

#include "TextComponent.h"
#include "ScrollableContainer.h"

class ThemeData;

// Hybrid class: inherits from TextComponent and ScrollableContainer.
// Maintains the general GuiComponent interface.
class ScrollableTextComponent : public TextComponent, public ScrollableContainer
{
public:
	ScrollableTextComponent(Window* window);

	void render(const Eigen::Affine3f& parentTrans) override;
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	unsigned char getOpacity() const override;
	void setOpacity(unsigned char opacity) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;
	std::string getValue() const override;
	void setValue(const std::string& value) override;

private:
	TextComponent mText;
	ScrollableContainer mContainer;
};

#endif
