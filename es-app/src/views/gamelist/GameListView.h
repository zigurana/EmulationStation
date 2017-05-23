#pragma once

#include "views/gamelist/ISimpleGameListView.h"
#include "components/TextListComponent.h"

class GameListView : public ISimpleGameListView
{
public:
	GameListView(Window* window, FileData* root, std::string view, std::shared_ptr<ThemeData> theme);

	// Called when a FileData* is added, has its metadata changed, or is removed
	//virtual void onFileChanged(FileData* file, FileChangeType change);

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual FileData* getCursor() override;
	virtual void setCursor(FileData* file) override;

	virtual const std::string getName() const override { return mViewName; }

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	virtual void populateList(const std::vector<FileData*>& files) override;
	
	virtual void launch(FileData* game) override;

//	virtual void remove(FileData* game) override;

	void applyThemes(const std::shared_ptr<ThemeData>& theme); // Apply the specified theme to all mThemeComponents.
	void setDefaultList();					// Sets the default for the only required GuiComponent for any view.
	void setComponentLegacyProperties();	// Set the properties of legacy theme element components (called once upon loading theme)
	void setComponentLegacyLabelValues();	// link the theme element name and value for legacy metadata labels (called once upon loading theme)
	void updateComponentValues();			// Update game specific theme-components (all in mMetaDataMapping)
	void makeThemeComponents(const std::shared_ptr<ThemeData>& theme);
	void fadeComponent(GuiComponent* comp, bool fadingOut);


	TextListComponent<FileData*> mList;
	std::map<std::string, GuiComponent*> mThemeComponents;// maps theme element names (first) to Guicomponents (second)
	std::map<std::string, std::string> mMetaDataMapping;  // maps theme element names (first) to MetaDataLabels (second)
	std::string mViewName;
	std::shared_ptr<ThemeData> mTheme;
};
