#include "animations/LambdaAnimation.h"
#include "views/gamelist/GameListView.h"
#include "views/ViewController.h"
#include "FileFilterIndex.h"
#include "MetaData.h"
#include "Renderer.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "Window.h"

GameListView::GameListView(Window* window, FileData* root, std::string view, std::shared_ptr<ThemeData> theme)
	: ISimpleGameListView(window, root),
	mList(window),
	mViewName(view),
	mTheme(theme)
{
	setDefaultList();
	populateList(root->getChildrenListToDisplay());
	onThemeChanged(theme); 
	updateComponentValues();
}

void GameListView::populateList(const std::vector<FileData*>& files)
{
	mList.clear();
	if (files.size() > 0)
	{
		for (auto it = files.begin(); it != files.end(); it++)
		{
			mList.add((*it)->getName(), *it, ((*it)->getType() == FOLDER));
		}
	}
	else
	{
		// empty list - add a placeholder
		FileData* placeholder = new FileData(PLACEHOLDER, "<No Results Found for Current Filter Criteria>", this->mRoot->getSystem());
		mList.add(placeholder->getName(), placeholder, (placeholder->getType() == PLACEHOLDER));
	}
}

void GameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	mList.applyTheme(theme, getName(), "gamelist", ThemeFlags::ALL);

	makeThemeComponents(theme);
	setComponentLegacyLabelValues();
	setComponentLegacyProperties();
	applyThemes(theme);
	sortChildren(); // sorts by z-index
}

FileData* GameListView::getCursor()
{
	// This is the mList (as opposed to gridview) specific implementation
	return mList.getSelected();
}

void GameListView::setCursor(FileData* cursor)
{
	if (cursor->isPlaceHolder())
		return;
	if(!mList.setCursor(cursor))
	{
		populateList(cursor->getParent()->getChildrenListToDisplay());
		mList.setCursor(cursor);

		// update our cursor stack in case our cursor just got set to some folder we weren't in before
		if(mCursorStack.empty() || mCursorStack.top() != cursor->getParent())
		{
			std::stack<FileData*> tmp;
			FileData* ptr = cursor->getParent();
			while(ptr && ptr != mRoot)
			{
				tmp.push(ptr);
				ptr = ptr->getParent();
			}

			// flip the stack and put it in mCursorStack
			mCursorStack = std::stack<FileData*>();
			while(!tmp.empty())
			{
				mCursorStack.push(tmp.top());
				tmp.pop();
			}
		}
	}
}

void GameListView::launch(FileData* game)
{
	// if no images / videos, just zoom on center
	Eigen::Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);
	
	// if md_video, then target on video,
	// else if md_image, then target on image
	// else if md_marquee, then target on marquee
	//assume origin is set to [0.5,0.5]

	auto comp = mThemeComponents.find("md_marquee");
	if ((comp != mThemeComponents.end()) && (comp->second->getValue() == "true"))
	{
		target = comp->second->getPosition();
	}
	comp = mThemeComponents.find("md_image");
	if ((comp != mThemeComponents.end()) && (comp->second->getValue() == "true"))
	{
		target = comp->second->getPosition();
	}
	comp = mThemeComponents.find("md_video");
	if ((comp != mThemeComponents.end()) && (comp->second->getValue() == "true"))
	{
		target = comp->second->getPosition();
	}
	
	ViewController::get()->launch(game, target);
}

std::vector<HelpPrompt> GameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if(Settings::getInstance()->getBool("QuickSystemSelect"))
		prompts.push_back(HelpPrompt("left/right", "system"));
	prompts.push_back(HelpPrompt("up/down", "choose"));
	prompts.push_back(HelpPrompt("a", "launch"));
	prompts.push_back(HelpPrompt("b", "back"));
	prompts.push_back(HelpPrompt("select", "options"));
	prompts.push_back(HelpPrompt("x", "random"));
	return prompts;
}

void GameListView::setDefaultList()
{
	mList.setSize(mSize.x(), mSize.y() * 0.8f);
	mList.setPosition(0, mSize.y() * 0.2f);
	mList.setCursorChangedCallback([&](const CursorState& state) { updateComponentValues(); });
	mList.setDefaultZIndex(20);
	addChild(&mList); 
}

void GameListView::updateComponentValues()
{
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();

	if (file != NULL)
	{
		// Loop trough all components, and those that are game-specific should be updated.
		for (auto mmap : mMetaDataMapping)
		{
			auto comp = mThemeComponents.find(mmap.first);
			if (comp != mThemeComponents.end())
			{
				comp->second->setValue(file->metadata.get(mmap.second));
			}
		}
	}
}

void GameListView::makeThemeComponents(const std::shared_ptr<ThemeData>& theme)
{	
	// destroy all mThemeComponents, then create new ones.
	for (auto comp : mThemeComponents)
	{
		removeChild(comp.second);
		delete comp.second;
	}
	mThemeComponents.clear();
	mMetaDataMapping.clear();

	mThemeComponents = ThemeData::makeThemeComponents(theme, mViewName, mWindow);
	for (auto component : mThemeComponents)
	{
		addChild(component.second);
	}
}

void GameListView::applyThemes(const std::shared_ptr<ThemeData>& theme)
{
	for (auto comp : mThemeComponents)
	{
		comp.second->applyTheme(theme, mViewName, comp.first, ThemeFlags::ALL);
	}
}

void GameListView::setComponentLegacyProperties()
{
	for (auto comp : mThemeComponents)
	{
		std::string metaDataLabel = "";

		if (comp.first == "background")
		{
			comp.second->setDefaultZIndex(0.f);
			comp.second->setZIndex(0.f);
		}
		if (comp.first == "md_title")
		{
			metaDataLabel = "name";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "gamelist")
		{
			comp.second->setDefaultZIndex(50);
			comp.second->setZIndex(50);
		}
		if (comp.first == "md_description")
		{
			metaDataLabel = "desc";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_image")
		{
			metaDataLabel = "image";
			comp.second->setZIndex(30);
			comp.second->setDefaultZIndex(30);
		}
		if (comp.first == "md_video")
		{
			metaDataLabel = "video";
			comp.second->setZIndex(30);
			comp.second->setDefaultZIndex(30);
		}
		if (comp.first == "md_marquee")
		{
			metaDataLabel = "marquee";
			comp.second->setZIndex(40);
			comp.second->setDefaultZIndex(40);
		}
		if (comp.first == "md_thumbnail")
		{
			metaDataLabel = "thumbnail";
			comp.second->setZIndex(40);
			comp.second->setDefaultZIndex(40);
		}
		if (comp.first == "md_rating")
		{
			metaDataLabel = "rating";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_releasedate")
		{
			metaDataLabel = "releasedate";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_developer")
		{
			metaDataLabel = "developer";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_publisher")
		{
			metaDataLabel = "publisher";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_genre")
		{
			metaDataLabel = "genre";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_players")
		{
			metaDataLabel = "players";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_playcount")
		{
			metaDataLabel = "playcount";
			comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_lastplayed")
		{
			metaDataLabel = "lastplayed";
			comp.second->setDefaultZIndex(50);
		}

		// For now remove unsupported MetaData types from collection.
		mThemeComponents.erase("md_hidden");
		mThemeComponents.erase("md_favorite");
		mThemeComponents.erase("md_kidgame");
		/*
		if (comp.first == "md_hidden")
		{
		metaDataLabel = "hidden";
		comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_favorite")
		{
		metaDataLabel = "favorite";
		comp.second->setDefaultZIndex(50);
		}
		if (comp.first == "md_kidgame")
		{
		metaDataLabel = "kidgame";
		comp.second->setDefaultZIndex(50);
		}
		*/

		// if item is identified based on the element name, add to list
		if (metaDataLabel != "")
		{
			std::pair<std::string, std::string> mmap(comp.first, metaDataLabel);
			mMetaDataMapping.insert(mmap);
		}
	}
}

void GameListView::setComponentLegacyLabelValues()
{
	for (auto comp : mThemeComponents)
	{
		if (comp.first == "md_lbl_rating")
		{
			comp.second->setValue("Rating: ");
		}
		else if (comp.first == "md_lbl_releasedate")
		{
			comp.second->setValue("Released: ");
		}
		else if (comp.first == "md_lbl_developer")
		{
			comp.second->setValue("Developer: ");
		}
		else if (comp.first == "md_lbl_publisher")
		{
			comp.second->setValue("Publisher: ");
		}
		else if (comp.first == "md_lbl_genre")
		{
			comp.second->setValue("Genre: ");
		}
		else if (comp.first == "md_lbl_players")
		{
			comp.second->setValue("Players: ");
		}
		else if (comp.first == "md_lbl_lastplayed")
		{
			comp.second->setValue("Last played: ");
		}
		else if (comp.first == "md_lbl_playcount")
		{
			comp.second->setValue("Times played: ");
		}
		else
		{
			// do nothing?
		}
	}
}