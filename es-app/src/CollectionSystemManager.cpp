#include "CollectionSystemManager.h"

#include "guis/GuiInfoPopup.h"
#include "views/gamelist/IGameListView.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include "ThemeData.h"
#include "Util.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <pugixml/src/pugixml.hpp>
#include <fstream>
#include <unordered_map>

namespace fs = boost::filesystem;
std::string myCollectionsName = "collections";
std::string defaultEditingCollection = "favorite";

/* Handling the getting, initialization, deinitialization, saving and deletion of
 * a CollectionSystemManager Instance */
CollectionSystemManager* CollectionSystemManager::sInstance = NULL;

CollectionSystemManager::CollectionSystemManager(Window* window) : mWindow(window), mIsEditing(false), mCustomCollectionsBundle(NULL)
{
	createCollectionSystemDeclarations();

	getStandardEnvironment();

	std::string path = getCollectionsFolder();
	if (!fs::exists(path))
	{
		fs::create_directory(path);
	}

	if (mCollectionSystems.find(defaultEditingCollection) != mCollectionSystems.end() &&
		mCollectionSystems.at(defaultEditingCollection)->isEnabled)
	{
		setEditMode(defaultEditingCollection);
	}
}

void CollectionSystemManager::createCollectionSystemDeclarations()
{
	CollectionSystemDecl systemDecls[] = {
		//type               name               long name            default sort                theme folder               isCustom     isEditable
		{ AUTO_ALL_GAMES,    "all",             "all games",         "filename, ascending",      "auto-allgames",           false,       false },
		{ AUTO_LAST_PLAYED,  "recent",          "last played",       "last played, descending",  "auto-lastplayed",         false,       false },
		{ AUTO_FAVORITES,    "favorite",        "favorites",         "filename, ascending",      "auto-favorites",          false,       true },
		{ AUTO_HIDDEN,       "hidden",          "items to hide",     "filename, ascending",      "custom-collections",      false,       true },
		{ AUTO_KIDGAME,      "kidgame",         "kid-friendly",      "filename, ascending",      "custom-collections",      false,       true },
		{ CUSTOM_COLLECTION, myCollectionsName, "collections",       "filename, ascending",      "custom-collections",      true,        true }
	};
	// NB: the 'name' used above should be the same as the metadata entry 'key', 
	// because it is used verbatim when toggling games!

	// create a map
	std::vector<CollectionSystemDecl> tempSystemDecl = std::vector<CollectionSystemDecl>(systemDecls, systemDecls + sizeof(systemDecls) / sizeof(systemDecls[0]));
	for (auto it : tempSystemDecl)
	{
		mCollectionSystemDeclsIndex[(it).name] = it;
	}
}

void CollectionSystemManager::getStandardEnvironment()
{
	// creating standard environment data
	mCollectionEnvData = new SystemEnvironmentData;
	mCollectionEnvData->mStartPath = "";
	std::vector<std::string> exts;
	mCollectionEnvData->mSearchExtensions = exts;
	mCollectionEnvData->mLaunchCommand = "";
	std::vector<PlatformIds::PlatformId> allPlatformIds;
	allPlatformIds.push_back(PlatformIds::PLATFORM_IGNORE);
	mCollectionEnvData->mPlatformIds = allPlatformIds;
}

CollectionSystemManager::~CollectionSystemManager()
{
	assert(sInstance == this);
	removeCollectionsFromDisplayedSystems();

	// iterate the map, save and destroy
	for (auto collection : mCollectionSystems)
	{
		if (collection.second->isPopulated)
		{
			collection.second->saveCollection(collection.second->system);
		}
		delete collection.second->system;
	}
	sInstance = NULL;
}

CollectionSystemManager* CollectionSystemManager::get()
{
	assert(sInstance);
	return sInstance;
}

void CollectionSystemManager::init(Window* window)
{
	assert(!sInstance);
	sInstance = new CollectionSystemManager(window);
}

void CollectionSystemManager::deinit()
{
	if (sInstance)
	{
		delete sInstance;
	}
}

/* Methods to load all Collections into memory, and handle enabling the active ones */
// loads all Collection Systems
void CollectionSystemManager::loadCollectionSystems()
{
	initAutoCollections();
	initCustomCollections();

	CollectionSystemDecl decl = mCollectionSystemDeclsIndex[myCollectionsName];
	mCustomCollectionsBundle = getNewCollection(decl.name, decl, false);
	initEnabledSystems();
}

void CollectionSystemManager::initEnabledSystems()
{
	if (Settings::getInstance()->getString("EnabledCollectionSystems") != "")
	{
		// Now see which ones are enabled
		loadEnabledListFromSettings();
		// add to the main System Vector, and create Views as needed
		updateSystemsList();
	}
}

// loads Automatic Collection systems (All, Favorites, Last Played, Hidden, Kidgame)
// initial loading of collections based on Metadata values
void CollectionSystemManager::initAutoCollections()
{
	for (auto collSystemDecl : mCollectionSystemDeclsIndex)
	{
		CollectionSystemDecl sysDecl = collSystemDecl.second;
		if (!sysDecl.isCustom)
		{
			getNewCollection(sysDecl.name, sysDecl);
		}
	}
}

void CollectionSystemManager::initCustomCollections()
{
	std::vector<std::string> customSystems = getCollectionsFromConfigFolder();
	for (auto systems : customSystems)
	{
		addNewCustomCollection(systems);
	}
}

// Returns which collection config files exist in the user folder
std::vector<std::string> CollectionSystemManager::getCollectionsFromConfigFolder()
{
	std::vector<std::string> systems;
	fs::path configPath = getCollectionsFolder();

	if (fs::exists(configPath))
	{
		fs::directory_iterator end_itr; // default construction yields past-the-end
		for (fs::directory_iterator itr(configPath); itr != end_itr; ++itr)
		{
			if (fs::is_regular_file(itr->status()))
			{
				// it's a file
				std::string file = itr->path().string();
				std::string filename = file.substr(configPath.string().size());

				// need to confirm filename matches config format
				if (boost::algorithm::ends_with(filename, ".cfg") && boost::algorithm::starts_with(filename, "custom-") && filename != "custom-.cfg")
				{
					filename = filename.substr(7, filename.size() - 11);
					systems.push_back(filename);
				}
				else
				{
					LOG(LogInfo) << "Found non-collection config file in collections folder: " << filename;
				}
			}
		}
	}
	return systems;
}

SystemData* CollectionSystemManager::addNewCustomCollection(std::string name)
{
	CollectionSystemDecl decl = mCollectionSystemDeclsIndex[myCollectionsName];
	decl.themeFolder = name;
	decl.name = name;
	decl.longName = name;
	return getNewCollection(name, decl);
}


// loads settings
void CollectionSystemManager::loadEnabledListFromSettings()
{
	std::vector<std::string> enabledSystems = commaStringToVector(Settings::getInstance()->getString("EnabledCollectionSystems"));

	// iterate the map
	for(auto & collection : mCollectionSystems)
	{
		collection.second->isEnabled = (std::find(enabledSystems.begin(), enabledSystems.end(), collection.first) != enabledSystems.end());
	}
}

// updates enabled system list in System View
void CollectionSystemManager::updateSystemsList()
{
	// remove all Collection Systems
	removeCollectionsFromDisplayedSystems();
	// add enabled ones
	addEnabledCollectionsToDisplayedSystems();

	if(Settings::getInstance()->getBool("sortCollectionsWithSystems"))
		std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), mixedSystemSort);
	else
		std::sort(SystemData::sSystemVector.begin(), SystemData::sSystemVector.end(), splitSystemSort);

	// create views for collections, before reload
	for (auto sysIt = SystemData::sSystemVector.begin(); sysIt != SystemData::sSystemVector.end(); sysIt++)
	{
		if ((*sysIt)->isCollection())
		{
			ViewController::get()->getGameListView((*sysIt));
		}
	}

	// if we were editing a collection, and it's no longer enabled, exit edit mode
	if(mEditingCollection != "" && !mEditingCollectionSystem->isEnabled)
	{
		exitEditMode();
	}
}

/* Methods to manage collection files related to a source FileData */
// updates all collection files related to the source file
void CollectionSystemManager::refreshCollectionSystems(FileData* file)
{
	if (!file->getSystem()->isGameSystem())
		return;

	for(auto & collectionSystem : mCollectionSystems)
	{
		collectionSystem.second->updateCollection(file);
	}
}

void CollectionSystemManager::CollectionSystem::updateCollection(FileData* file)
{
	if (this->isPopulated)
	{
		// collection files use the full path as key, to avoid clashes
		std::string key = file->getFullPath();

		SystemData* systemData = this->system;
		const std::unordered_map<std::string, FileData*>& collectionContent = systemData->getRootFolder()->getChildrenByFilename();
		FileFilterIndex* collectionIndex = systemData->getIndex();

		// check if file is part of a collection
		if (collectionContent.find(key) != collectionContent.end()) {
			FileData* collectionEntry = collectionContent.at(key);
			// remove from index, so we can re-index metadata after refreshing
			collectionIndex->removeFromIndex(collectionEntry);
			collectionEntry->refreshMetadata();  // get metadata from original system entry

			if(isValidGame(file))
			{
				// re-index with new metadata
				collectionIndex->addToIndex(collectionEntry);
				ViewController::get()->onFileChanged(collectionEntry, FILE_METADATA_CHANGED);
			}
			else
			{ //no longer valid; remove
				this->removeGameFromCollection(file);
				ViewController::get()->getGameListView(systemData).get()->remove(collectionEntry, false);
			}
		}
		else
		{
			// we didn't find it here - we need to check if we should add it
			if (isValidGame(file))
			{
				addGameToCollection(file);
			}
		}

		this->resetSort();
		this->isPopulated = true;
	}
}

void CollectionSystemManager::CollectionSystem::resetSort()
{
	this->system->getRootFolder()->sort(getSortTypeFromString(this->decl.defaultSort));
	SystemData* systemViewToUpdate = CollectionSystemManager::get()->getSystemToView(this->system);
	ViewController::get()->onFileChanged(systemViewToUpdate->getRootFolder(), FILE_SORTED);
}


// deletes all collection files from collection systems related to the source file
void CollectionSystemManager::deleteCollectionFiles(FileData* file)
{
	// collection files use the full path as key, to avoid clashes
	std::string key = file->getFullPath();

	// find games in collection systems
	for(auto & system : mCollectionSystems)
	{
		if (system.second->isPopulated)
		{
			const std::unordered_map<std::string, FileData*>& children = (system.second->system)->getRootFolder()->getChildrenByFilename();

			bool found = children.find(key) != children.cend();
			if (found) {
				system.second->needsSave = true;
				FileData* collectionEntry = children.at(key);
				SystemData* systemViewToUpdate = getSystemToView(system.second->system);
				ViewController::get()->getGameListView(systemViewToUpdate).get()->remove(collectionEntry, false);
			}
		}
	}
}

std::string CollectionSystemManager::getValidNewCollectionName(std::string inName, int index)
{
	// filter name - [^A-Za-z0-9\[\]\(\)\s]
	using namespace boost::xpressive;
	std::string name;
	sregex regexp = sregex::compile("[^A-Za-z0-9\\-\\[\\]\\(\\)\\s']");
	if (index == 0)
	{
		name = regex_replace(inName, regexp, "");
		if (name == "")
		{
			name = "New Collection";
		}
	}
	else
	{
		name = inName + " (" + std::to_string(index) + ")";
	}
	if(name != inName)
	{
		LOG(LogInfo) << "Had to change name, from: " << inName << " to: " << name;
	}

	for(auto system : getSystemsInUse())
	{
		if (system == name)
		{
			if(index > 0) {
				name = name.substr(0, name.size()-4);
			}
			return getValidNewCollectionName(name, index+1);
		}
	}
	// if it matches one of the collections reserved names
	if (mCollectionSystemDeclsIndex.find(name) != mCollectionSystemDeclsIndex.end())
		return getValidNewCollectionName(name, index+1);
	return name;
}

void CollectionSystemManager::setEditMode(std::string collectionName)
{

	if (mCollectionSystems.find(collectionName) == mCollectionSystems.end() ||
		!(mCollectionSystems.at(collectionName)->isEnabled))
	{
		LOG(LogError) << "Tried to edit a non-existing or disabled collection: " << collectionName;
		mIsEditing = false;
		return;
	}

	mEditingCollection = collectionName;
	mEditingCollectionSystem = mCollectionSystems.at(mEditingCollection);
	if (!mEditingCollectionSystem->isPopulated)
	{
		mEditingCollectionSystem->populate();
	}

	GuiInfoPopup* s = new GuiInfoPopup(mWindow, "Editing the '" + strToUpper(collectionName) + "' Collection. Add/remove games with Y.", 10000);
	mWindow->setInfoPopup(s);
	mIsEditing = true;
}

void CollectionSystemManager::exitEditMode()
{
	GuiInfoPopup* s = new GuiInfoPopup(mWindow, "Finished editing the '" + mEditingCollection + "' Collection.", 4000);
	mWindow->setInfoPopup(s);
	setEditMode(defaultEditingCollection);  // default edit mode = favorites 
	mEditingCollection = "Favorites";		// set editingmode to favorites, even if not enabled as a collection
}

bool CollectionSystemManager::toggleGameInCollection(FileData* file)
{

	return mEditingCollectionSystem->toggleGameInCollection(file);
}

// Determine if system has its own view, or if its part of CustomCollectionsBundle
SystemData* CollectionSystemManager::getSystemToView(SystemData* sys)
{
	SystemData* systemToView = sys;
	FileData* rootFolder = sys->getRootFolder();

	FileData* bundleRootFolder = mCustomCollectionsBundle->getRootFolder();
	const std::unordered_map<std::string, FileData*>& bundleChildren = bundleRootFolder->getChildrenByFilename();

	// is the rootFolder bundled in the "My Collections" system?
	bool sysFoundInBundle = bundleChildren.find(rootFolder->getKey()) != bundleChildren.cend();

	if (sysFoundInBundle && sys->isCollection())
	{
		systemToView = mCustomCollectionsBundle;
	}
	return systemToView;
}

// automatically generate metadata for a folder
void CollectionSystemManager::CollectionSystem::updateCollectionFolderMetadata()
{
	FileData* rootFolder = this->system->getRootFolder();
	// TODO: after Theming refactoring, get default MDD from metadata.cpp
	std::string desc = "This collection is empty.";
	std::string rating = "0";
	std::string players = "1";
	std::string releasedate = "N/A";
	std::string developer = "None";
	std::string genre = "None";
	std::string video = "";
	std::string thumbnail = "";

	std::unordered_map<std::string, FileData*> games = rootFolder->getChildrenByFilename();

	if(games.size() > 0)
	{
		std::string games_list = "";
		int games_counter = 0;
		for(std::unordered_map<std::string, FileData*>::const_iterator iter = games.cbegin(); iter != games.cend(); ++iter)
		{
			games_counter++;
			FileData* file = iter->second;

			std::string new_rating = file->metadata.get("rating");
			std::string new_releasedate = file->metadata.get("releasedate");
			std::string new_developer = file->metadata.get("developer");
			std::string new_genre = file->metadata.get("genre");
			std::string new_players = file->metadata.get("players");

			rating = (new_rating > rating ? (new_rating != "" ? new_rating : rating) : rating);
			players = (new_players > players ? (new_players != "" ? new_players : players) : players);
			releasedate = (new_releasedate < releasedate ? (new_releasedate != "" ? new_releasedate : releasedate) : releasedate);
			developer = (developer == "None" ? new_developer : (new_developer != developer ? "Various" : new_developer));
			genre = (genre == "None" ? new_genre : (new_genre != genre ? "Various" : new_genre));

			switch(games_counter)
			{
				case 2:
				case 3:
					games_list += ", ";
				case 1:
					games_list += "'" + file->getName() + "'";
					break;
				case 4:
					games_list += " among other titles.";
			}
		}

		desc = "This collection contains " + std::to_string(games_counter) + " games, including " + games_list;

		FileData* randomGame = this->system->getRandomGame();

		video = randomGame->getVideoPath();
		thumbnail = randomGame->getThumbnailPath();
	}


	rootFolder->metadata.set("desc", desc);
	rootFolder->metadata.set("rating", rating);
	rootFolder->metadata.set("players", players);
	rootFolder->metadata.set("genre", genre);
	rootFolder->metadata.set("releasedate", releasedate);
	rootFolder->metadata.set("developer", developer);
	rootFolder->metadata.set("video", video);
	rootFolder->metadata.set("image", thumbnail);
}

SystemData* CollectionSystemManager::getAllGamesCollection()
{
	CollectionSystem* allSystem = mCollectionSystems["all"];
	if (!allSystem->isPopulated)
	{
		allSystem->populate();
	}
	return allSystem->system;
}


// creates a new, empty (Custom) Collection system, based on the name and declaration and adds to list.
SystemData* CollectionSystemManager::getNewCollection(std::string name, CollectionSystemDecl sysDecl, bool index)
{
	SystemData* newSys = new SystemData(name, sysDecl.longName, mCollectionEnvData, sysDecl.themeFolder, true);

	CollectionSystem * newCollection;
	if (sysDecl.isCustom)
		newCollection = new CustomCollectionSystem;
	else
		newCollection = new AutoCollectionSystem;

	newCollection->system = newSys;
	newCollection->decl = sysDecl;
	newCollection->isEnabled = false;
	newCollection->isPopulated = false;
	newCollection->needsSave = false;

	if (index)
	{
		mCollectionSystems[name] = newCollection;
	}

	return newSys;
}

// populates an Automatic Collection System
void CollectionSystemManager::AutoCollectionSystem::populate()
{
	for (auto system : SystemData::sSystemVector)
	{
		// we won't iterate all collections
		if (system->isGameSystem() && !system->isCollection())
		{
			std::vector<FileData*> files = system->getRootFolder()->getFilesRecursive(GAME);
			for (const auto game : files)
			{
				if (isValidGame(game))
				{
					addGameToCollection(game);
				}
			}
		}
	}
	resetSort();
	this->isPopulated = true;
}


void CollectionSystemManager::CollectionSystem::addGameToCollection(FileData* game)
{
	CollectionFileData* newGame = new CollectionFileData(game, this->system);
	this->system->getRootFolder()->addChild(newGame);
	FileFilterIndex* index = this->system->getIndex();
	index->addToIndex(newGame);
	SystemData* systemViewToUpdate = CollectionSystemManager::get()->getSystemToView(this->system);

	// add to bundle index as well, if needed
	if (systemViewToUpdate != this->system)
	{
		systemViewToUpdate->getIndex()->addToIndex(newGame);
	}
	ViewController::get()->onFileChanged(game, FILE_METADATA_CHANGED);
	ViewController::get()->getGameListView(systemViewToUpdate)->onFileChanged(newGame, FILE_METADATA_CHANGED);
}

bool CollectionSystemManager::CollectionSystem::isValidGame(FileData* game)
{
	bool valid = game->isGameFile();
	switch (this->decl.type) {
	case AUTO_LAST_PLAYED:
		valid = valid && game->metadata.getInt("playcount") > 0;
		break;
	case AUTO_FAVORITES:
		// we may still want to add non-games to "favorites"
		valid = game->metadata.getBool("favorite");
		break;
	case AUTO_HIDDEN:
		valid = game->metadata.getBool("hidden"); // also include non-games
		break;
	case AUTO_KIDGAME:
		valid = game->metadata.getBool("kidgame");
		break;

	}
	return valid;
}

// populates a Custom Collection System
void CollectionSystemManager::CustomCollectionSystem::populate()
{
	
	std::string path = getCustomCollectionConfigPath(this->system->getName());
	if (path == "")
		return; // path not found

	FileData* rootFolder = this->system->getRootFolder();
	FileFilterIndex* index = this->system->getIndex();

	// get Configuration for this Custom System
	std::ifstream input(path);

	// get map of all files
	std::unordered_map<std::string,FileData*> allFilesMap = CollectionSystemManager::get()->getAllGamesCollection()->getRootFolder()->getChildrenByFilename();

	// iterate list of files in config file
	for(std::string gameKey; getline(input, gameKey); )
	{
		auto game = allFilesMap.find(gameKey);
		if (game != allFilesMap.end())
		{
			addGameToCollection(game->second);
		}
		else
		{
			LOG(LogInfo) << "Couldn't find game referenced at '" << gameKey << "' for system config '" << path << "'";
		}
	}
	
	this->resetSort();
	this->updateCollectionFolderMetadata();
	this->isPopulated = true;
}

/* Handle System View removal and insertion of Collections */
void CollectionSystemManager::removeCollectionsFromDisplayedSystems()
{
	// remove all Collection Systems
	for(auto sysIt = SystemData::sSystemVector.cbegin(); sysIt != SystemData::sSystemVector.cend(); )
	{
		if ((*sysIt)->isCollection())
		{
			sysIt = SystemData::sSystemVector.erase(sysIt);
		}
		else
		{
			sysIt++;
		}
	}

	// remove all custom collections in bundle
	// this should not delete the objects from memory!
	FileData* customRoot = mCustomCollectionsBundle->getRootFolder();
	std::vector<FileData*> mChildren = customRoot->getChildren();
	for(auto it = mChildren.cbegin(); it != mChildren.cend(); it++)
	{
		customRoot->removeChild(*it);
	}
	// clear index
	mCustomCollectionsBundle->getIndex()->resetIndex();
	// remove view so it's re-created as needed
	ViewController::get()->removeGameListView(mCustomCollectionsBundle);
}

void CollectionSystemManager::addEnabledCollectionsToDisplayedSystems()
{
	// add auto enabled ones
	for (auto & collection : mCollectionSystems)
	{
		if(collection.second->isEnabled)
		{
			// check if populated, otherwise populate
			if (!collection.second->isPopulated)
			{
				collection.second->populate();
			}
			// check if it has its own view
			if(collection.second->decl.isCustom || themeFolderExists(collection.first) || !Settings::getInstance()->getBool("UseCustomCollectionsSystem"))
			{
				// exists theme folder, or we chose not to bundle it under the custom-collections system
				// so we need to create a view
				SystemData::sSystemVector.push_back(collection.second->system);
			}
			else // its part of the 'collections' system (bundle)
			{
				FileData* newSysRootFolder = collection.second->system->getRootFolder();
				mCustomCollectionsBundle->getRootFolder()->addChild(newSysRootFolder);
				mCustomCollectionsBundle->getIndex()->importIndex(collection.second->system->getIndex());
			}
		}
	}

	// after populating, sort the bundled collections
	if (mCustomCollectionsBundle->getRootFolder()->getChildren().size() > 0)
	{
		SystemData::sSystemVector.push_back(mCustomCollectionsBundle);
		mCustomCollectionsBundle->getRootFolder()->sort(getSortTypeFromString(mCollectionSystemDeclsIndex[myCollectionsName].defaultSort));
	}
}

/* Auxiliary methods to get available custom collection possibilities */
std::vector<std::string> CollectionSystemManager::getSystemsFromConfig()
{
	std::vector<std::string> systems;

	std::string path = SystemData::getConfigPath(false);

	if(!fs::exists(path))
	{
		return systems;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if(!res)
	{
		return systems;
	}

	//actually read the file
	pugi::xml_node systemList = doc.child("systemList");

	if(!systemList)
	{
		return systems;
	}

	for(pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		// theme folder
		std::string themeFolder = system.child("theme").text().get();
		systems.push_back(themeFolder);
	}
	std::sort(systems.begin(), systems.end());
	return systems;
}

// gets all folders from the current theme path
std::vector<std::string> CollectionSystemManager::getSystemsFromTheme()
{
	std::vector<std::string> systems;

	auto themeSets = ThemeData::getThemeSets();
	if(themeSets.empty())
	{
		// no theme sets available
		return systems;
	}

	std::map<std::string, ThemeSet>::const_iterator set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.cend())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.cbegin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	fs::path themePath = set->second.path;

	if (fs::exists(themePath))
	{
		fs::directory_iterator end_itr; // default construction yields past-the-end
		for (fs::directory_iterator itr(themePath); itr != end_itr; ++itr)
		{
			if (fs::is_directory(itr->status()))
			{
				//... here you have a directory
				std::string folder = itr->path().string();
				folder = folder.substr(themePath.string().size()+1);

				if(fs::exists(set->second.getThemePath(folder)))
				{
					systems.push_back(folder);
				}
			}
		}
	}
	std::sort(systems.begin(), systems.end());
	return systems;
}

// returns the unused folders from current theme path
std::vector<std::string> CollectionSystemManager::getUnusedSystemsFromTheme()
{
	std::vector<std::string> systemsInUse = getSystemsInUse();
	std::vector<std::string> themedSystems = getSystemsFromTheme();
	
	for(auto sysIt = themedSystems.begin(); sysIt != themedSystems.end(); )
	{
		if (std::find(systemsInUse.cbegin(), systemsInUse.cend(), *sysIt) != systemsInUse.cend())
		{
			sysIt = themedSystems.erase(sysIt);
		}
		else
		{
			sysIt++;
		}
	}
	return themedSystems;
}

std::vector<std::string> CollectionSystemManager::getSystemsInUse()
{
	// get used systems in es_systems.cfg
	std::vector<std::string> systemsInUse = getSystemsFromConfig();
	// get folders assigned to  collections
	std::vector<std::string> collectionThemeFolders = getCollectionThemeFolders();
	// add them all to the list of systems in use
	systemsInUse.insert(systemsInUse.end(), collectionThemeFolders.begin(), collectionThemeFolders.end());

	return systemsInUse;
}


std::vector<std::string> CollectionSystemManager::getEditableCollectionsNames()
{
	std::vector<std::string> colsSysDataNames;
	for (auto collection : mCollectionSystems)
	{
		if (collection.second->decl.isEditable == true)
		{
			colsSysDataNames.push_back(collection.first);
		}
	}
	return colsSysDataNames;
}

// returns the theme folders in use for Collections
std::vector<std::string> CollectionSystemManager::getCollectionThemeFolders()
{
	std::vector<std::string> themeFolders;
	for(auto collection : mCollectionSystems)
	{
		themeFolders.push_back(collection.second->decl.themeFolder);
	}
	return themeFolders;
}

// returns whether a specific folder exists in the theme
bool CollectionSystemManager::themeFolderExists(std::string folder)
{
	std::vector<std::string> themeSys = getSystemsFromTheme();
	return std::find(themeSys.cbegin(), themeSys.cend(), folder) != themeSys.cend();
}

std::string getCustomCollectionConfigPath(std::string collectionName)
{
	fs::path path = getCollectionsFolder() + "custom-" + collectionName + ".cfg";

	if (!fs::exists(path))
	{
		LOG(LogInfo) << "Couldn't find custom collection config file at " << path;
		return "";
	}
	LOG(LogDebug) << "Loading custom collection config file at " << path;

	return path.generic_string();
}

std::string getCollectionsFolder()
{
	return getHomePath() + "/.emulationstation/collections/";
}

bool splitSystemSort(SystemData* sys1, SystemData* sys2)
{
	// The goal here is to sort as follows:
	// 1- all regular systems (non-collection), in alphabetical order
	// 2- RetroPie system
	// 3- custom systems and auto systems, in alphabetical order

	std::string name1 = sys1->getName();
	std::string name2 = sys2->getName();
	strToUpper(name1);
	strToUpper(name2);

	if (sys1->isCollection() && !sys2->isCollection())
		return false;
	else if (!sys1->isCollection() && sys2->isCollection())
		return true;
	else if (!sys1->isCollection() && !sys2->isCollection())
	{
		if (name1 == "RETROPIE")
			return false;
		else if (name2 == "RETROPIE")
			return true;
	}
	
	return name1.compare(name2) < 0;		
}

bool mixedSystemSort(SystemData* sys1, SystemData* sys2)
{
	// The goal here is to sort as follows:
	// 1- all regular systems (non-collection) and custom collections, in alphabetical order
	// 2- RetroPie system
	// 3- auto systems, in fixed order: "RETROPIE", "COLLECTIONS", "ALL", "FAVORITE", "RECENT", "KIDGAME", "HIDDEN"
	std::string name1 = sys1->getName();
	std::string name2 = sys2->getName();
	strToUpper(name1);
	strToUpper(name2);

	std::vector<std::string> setApartList = { "RETROPIE", "COLLECTIONS", "ALL", "FAVORITE", "RECENT", "KIDGAME", "HIDDEN" };
	auto it1 = std::find(setApartList.begin(), setApartList.end(), name1);
	auto it2 = std::find(setApartList.begin(), setApartList.end(), name2);
	bool set1Apart = it1 != setApartList.end();
	bool set2Apart = it2 != setApartList.end();

	if (set1Apart && !set2Apart)
		return false;
	else if (!set1Apart && set2Apart)
		return true;
	else if(!set1Apart && !set2Apart)
		return name1.compare(name2) < 0;
	else
	{
		unsigned int dist1 = std::distance(setApartList.begin(), it1);
		unsigned int dist2 = std::distance(setApartList.begin(), it2);
		return dist1 <= dist2;
	}
}

// adds or removes a game from a collection
bool CollectionSystemManager::AutoCollectionSystem::toggleGameInCollection(FileData* file)
{
	if (file->getType() == GAME)  // TODO: consider: use file->isGameFile() instead?
	{
		MetaDataList* metadata = &file->getSourceFileData()->metadata;
		std::string key = this->decl.name;

		if (metadata->getBool(key))
		{// current metadata value == true, set to false -> remove from collection
			metadata->set(key, "false");
			showToggleInfoPopup(file->getName(), this->system->getName(), false);
			removeGameFromCollection(file);
		}
		else
		{// current metadata value == false, set to true -> add to collection
			metadata->set(key, "true");
			showToggleInfoPopup(file->getName(), this->system->getName(), true);
			addGameToCollection(file);
			this->resetSort();
		}

		updateCollectionFolderMetadata();
		return true;
	}
	return false;
}

void CollectionSystemManager::CollectionSystem::showToggleInfoPopup(std::string gameName, std::string sysName, bool added)
{
	Window * window = CollectionSystemManager::get()->mWindow;
	GuiInfoPopup* s;

	if (added)
	{
		s = new GuiInfoPopup(window, "Added '" + removeParenthesis(gameName) + "' to '" + strToUpper(sysName) + "'", 4000);
	}
	else
	{
		s = new GuiInfoPopup(window, "Removed '" + removeParenthesis(gameName) + "' from '" + strToUpper(sysName) + "'", 4000);
	}
	window->setInfoPopup(s);
}

bool CollectionSystemManager::CustomCollectionSystem::toggleGameInCollection(FileData* file)
{
	if (file->getType() == GAME)
	{
		this->needsSave = true;
		if (!this->isPopulated)
		{
			populate();
		}
		std::string key = file->getFullPath();
		FileData* rootFolder = this->system->getRootFolder();


		const std::unordered_map<std::string, FileData*>& children = rootFolder->getChildrenByFilename();
		auto collectionEntry = children.find(key);
		if (collectionEntry != children.end())
		{// found it, we need to remove it
			showToggleInfoPopup(file->getName(), this->system->getName(), false);
			removeGameFromCollection(collectionEntry->second);
		}
		else
		{// we didn't find it, we should add it
			addGameToCollection(file);
			this->resetSort();
			showToggleInfoPopup(file->getName(), this->system->getName(), true);

		}
		updateCollectionFolderMetadata();

		return true;
	}
	return false;
}

void CollectionSystemManager::CollectionSystem::removeGameFromCollection(FileData* game)
{
	// remove from index
	FileFilterIndex* fileIndex = this->system->getIndex();
	fileIndex->removeFromIndex(game);
	// remove from bundle index as well, if needed
	SystemData* systemViewToUpdate = CollectionSystemManager::get()->getSystemToView(this->system);
	if (systemViewToUpdate != this->system)
	{
		systemViewToUpdate->getIndex()->removeFromIndex(game);
	}
	ViewController::get()->getGameListView(systemViewToUpdate).get()->remove(game, false);
}


void CollectionSystemManager::CustomCollectionSystem::saveCollection()
{
	SystemData* sys = this->system;
	std::string name = sys->getName();
	std::unordered_map<std::string, FileData*> games = sys->getRootFolder()->getChildrenByFilename();

	if (this->decl.isCustom)  // we only need to save the custom collections, metadata changes are saved elsewhere
	{
		if (this->needsSave)
		{
			std::ofstream configFile;
			configFile.open(getCustomCollectionConfigPath(name));
			for (auto game : games)
			{
				std::string path = game.first;
				configFile << path << std::endl;
			}
			configFile.close();
		}
	}
	else
	{
		LOG(LogError) << "Couldn't find collection to save! " << name;
	}
}
