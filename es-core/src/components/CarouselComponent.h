#ifndef _CAROUSELCOMPONENT_H_
#define _CAROUSELCOMPONENT_H_

#include "platform.h"
#include GLHEADER

#include "GuiComponent.h"
#include <string>
#include <memory>
#include "resources/TextureResource.h"

class CarouselComponent : public GuiComponent
{
public:
	enum CarouselType : unsigned int
	{
		HORIZONTAL = 0,
		VERTICAL = 1
	};

							CarouselComponent(Window* window, bool forceLoad = false, bool dynamic = true);
	virtual					~CarouselComponent();
	void					render(const Eigen::Affine3f& parentTrans);

	Eigen::Vector2f			getCenter() const;
	void					setOrigin(float originX, float originY);

	virtual void			applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;
	
	bool					input(InputConfig* config, Input input); 
	
	std::vector<HelpPrompt> getHelpPrompts();

private:
	void					setDefaults();

	CarouselType			mType;			// Either HORIZONTAL or VERTICAL
	Eigen::Vector2f			mPos;
	Eigen::Vector2f			mCarouselSize;
	Eigen::Vector2f			mOrigin;

	float					mLogoScale;		// relative scale of the selected logo wrt the normal logo size
	Eigen::Vector2f			mLogoSpacing;
	unsigned int			mColor;
	int						mMaxLogoCount;	// number of logos shown on the carousel
	Eigen::Vector2f			mLogoSize;
	float					mZIndex;
};

#endif
