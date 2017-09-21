#pragma once
#ifndef ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H
#define ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H

#include "components/MenuComponent.h"

template<typename T>
class OptionListComponent;
class SwitchComponent;
class SystemData;

class GuiCollectionSystemsOptions : public GuiComponent
{
public:
	GuiCollectionSystemsOptions(Window* window);
	~GuiCollectionSystemsOptions();
	bool input(InputConfig* config, Input input) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void initializeMenu();
	void applySettings();
	void addSystemsToMenu();
	void addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func);
	void updateSettings(std::string newSelectedSystems);
	void createCollection(std::string inName);
	void exitEditMode();
	std::shared_ptr< OptionListComponent<std::string> > optionList;
	std::shared_ptr<SwitchComponent> sortCollectionsWithSystemsSwitch;
	std::shared_ptr<SwitchComponent> bundleCustomCollections;
	MenuComponent mMenu;
	SystemData* mSystem;
};

#endif // ES_APP_GUIS_GUI_COLLECTION_SYSTEM_OPTIONS_H
