#pragma once

#include "views/gamelist/ISimpleGameListView.h"
#include "components/TextListComponent.h"

class BasicGameListView : public ISimpleGameListView
{
public:
	BasicGameListView(Window* window, FileData* root);

	// Called when a FileData* is added, has its metadata changed, or is removed
	// virtual void onFileChanged(FileData* file, FileChangeType change); -> now in ISimpleGamelistView

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual FileData* getCursor() override;
	virtual void setCursor(FileData* file) override;

	virtual const std::string getName() const override { return "basic"; }

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual void launch(FileData* game) override;

protected:
	virtual void populateList(const std::vector<FileData*>& files) override;
	virtual void remove(FileData* game, bool deleteFile) override;
	virtual void addPlaceholder();

	TextComponent mHeaderText;
	ImageComponent mHeaderImage;
	ImageComponent mBackground;
	TextListComponent<FileData*> mList;
	std::vector<GuiComponent*> mThemeExtras;

private:
	void setDefaultHeaderText();
	void setDefaultBackground();
	void setDefaultHeaderImage();
	void setDefaultList();
	void refreshExtras(const std::shared_ptr<ThemeData>& theme);

};
