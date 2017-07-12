#include "views/gamelist/ISimpleGameListView.h"
#include "ThemeData.h"
#include "Window.h"
#include "views/ViewController.h"
#include "Sound.h"
#include "Log.h"
#include "Settings.h"
#include "CollectionSystemManager.h"

ISimpleGameListView::ISimpleGameListView(Window* window, FileData* root) : IGameListView(window, root),
	mHeaderText(window), mHeaderImage(window), mBackground(window)
{
	// This space has been intentionally left blank.
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	// This space has been intentionally left blank.
}

void ISimpleGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if (change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	// we could be tricky here to be efficient;
	// but this shouldn't happen very often so we'll just always repopulate
	FileData* cursor = getCursor();
	if (!cursor->isPlaceHolder()) {
		populateList(cursor->getParent()->getChildrenListToDisplay());
		setCursor(cursor);
	}
	else
	{
		populateList(mRoot->getChildrenListToDisplay());
		setCursor(cursor);
	}
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->isMappedTo("a", input))
		{
			FileData* cursor = getCursor();
			if(cursor->getType() == GAME)
			{
				Sound::getFromTheme(getTheme(), getName(), "launch")->play();
				launch(cursor);
			}else{
				// it's a folder
				if(cursor->getChildren().size() > 0)
				{
					mCursorStack.push(cursor);
					populateList(cursor->getChildrenListToDisplay());
				}
			}

			return true;
		}else if(config->isMappedTo("b", input))
		{
			if(mCursorStack.size())
			{
				populateList(mCursorStack.top()->getParent()->getChildren());
				setCursor(mCursorStack.top());
				mCursorStack.pop();
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
			}else{
				onFocusLost();
				ViewController::get()->goToSystemView(getCursor()->getSystem());
			}

			return true;
		}else if(config->isMappedTo("right", input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
				ViewController::get()->goToNextGameList();
				Sound::getFromTheme(getTheme(), getName(), "switch")->play();
				return true;
			}
		}else if(config->isMappedTo("left", input))
		{
			if(Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
				ViewController::get()->goToPrevGameList();
				Sound::getFromTheme(getTheme(), getName(), "switch")->play();
				return true;
			}
		}else if (config->isMappedTo("x", input))
		{
			// go to random system game
			setCursor(mRoot->getSystem()->getRandomGame());
			//ViewController::get()->goToRandomGame();
			Sound::getFromTheme(getTheme(), getName(), "random")->play();
			return true;
		}else if (config->isMappedTo("y", input))
		{
			if(Settings::getInstance()->getString("CollectionSystemsAuto").find("favorites") != std::string::npos && mRoot->getSystem()->isGameSystem())
			{
				if(CollectionSystemManager::get()->toggleGameInCollection(getCursor(), "favorites"))
				{
					return true;
				}
			}
		}
	}

	return IGameListView::input(config, input);
}
