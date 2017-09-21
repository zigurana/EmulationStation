#pragma once
#ifndef ES_APP_COLLECTION_SYSTEM_MANAGER_H
#define ES_APP_COLLECTION_SYSTEM_MANAGER_H

#include <map>
#include <string>
#include <vector>

class FileData;
class SystemData;
class Window;
struct SystemEnvironmentData;

enum CollectionSystemType
{
	AUTO_ALL_GAMES,
	AUTO_LAST_PLAYED,
	AUTO_FAVORITES,
	AUTO_HIDDEN,
	AUTO_KIDGAME,
	CUSTOM_COLLECTION
};

struct CollectionSystemDecl
{
	CollectionSystemType type; // type of system
	std::string name;
	std::string longName;
	std::string defaultSort;
	std::string themeFolder;
	bool isCustom;
	bool isEditable;
};



class CollectionSystemManager
{
public:
	class CollectionSystem
	{
	public:
		virtual bool toggleGameInCollection(FileData* file) { return false; };
		virtual void populate() { return; };
		virtual void saveCollection(SystemData* sys) {};
		void updateCollection(FileData* file);
		void updateCollectionFolderMetadata();

		SystemData* system;
		CollectionSystemDecl decl;
		bool isEnabled;
		bool isPopulated;
		bool needsSave;

	protected:
		bool isValidGame(FileData* game);
		void addGameToCollection(FileData* game);
		void removeGameFromCollection(FileData* game);
		void resetSort();
		void showToggleInfoPopup(std::string gameName, std::string sysName, bool added);
	};

	//typedef std::map<std::string, CollectionSystem*> NameCollectionMap;

	class AutoCollectionSystem : public CollectionSystem
	{
	public:
		bool toggleGameInCollection(FileData* file) override;
		void populate() override;
	
	};

	class CustomCollectionSystem : public CollectionSystem
	{
	public:
		bool toggleGameInCollection(FileData* file) override;
		void populate() override;
		void saveCollection();
	private:

	};

	CollectionSystemManager(Window* window);
	void createCollectionSystemDeclarations();
	void getStandardEnvironment();
	~CollectionSystemManager();

	static CollectionSystemManager* get();
	static void init(Window* window);
	static void deinit();
	

	void loadCollectionSystems();
	void initEnabledSystems();
	void loadEnabledListFromSettings();
	void updateSystemsList();

	void refreshCollectionSystems(FileData* file);
	void deleteCollectionFiles(FileData* file);

	inline std::map<std::string, CollectionSystem*> getCollectionSystems() { return mCollectionSystems; };

	std::vector<std::string> getEditableCollectionsNames();

	inline SystemData* getCustomCollectionsBundle() { return mCustomCollectionsBundle; };
	std::vector<std::string> getUnusedSystemsFromTheme();
	SystemData* addNewCustomCollection(std::string name);

	std::string getValidNewCollectionName(std::string name, int index = 0);
	std::vector<std::string> getSystemsInUse();

	void setEditMode(std::string collectionName);
	void exitEditMode();
	inline bool isEditing() { return mIsEditing; };
	inline std::string getEditingCollection() { return mEditingCollection; };
	bool toggleGameInCollection(FileData* file);

	SystemData* getSystemToView(SystemData* sys);

private:
	static CollectionSystemManager* sInstance;
	Window* mWindow;

	SystemEnvironmentData* mCollectionEnvData;
	std::map<std::string, CollectionSystemDecl> mCollectionSystemDeclsIndex;
	std::map<std::string, CollectionSystem*> mCollectionSystems;

	std::string mEditingCollection;
	CollectionSystem* mEditingCollectionSystem;
	bool mIsEditing;

	void init();
	void initAutoCollections();
	void initCustomCollections();
	SystemData* getAllGamesCollection();
	SystemData* getNewCollection(std::string name, CollectionSystemDecl sysDecl, bool index = true);

	void removeCollectionsFromDisplayedSystems();
	void addEnabledCollectionsToDisplayedSystems();

	std::vector<std::string> getSystemsFromConfig();
	std::vector<std::string> getSystemsFromTheme();
	std::vector<std::string> getCollectionsFromConfigFolder();
	std::vector<std::string> getCollectionThemeFolders();

	bool themeFolderExists(std::string folder);

	SystemData* mCustomCollectionsBundle;  
};

std::string getCustomCollectionConfigPath(std::string collectionName);
std::string getCollectionsFolder();

bool splitSystemSort(SystemData* sys1, SystemData* sys2);
bool mixedSystemSort(SystemData* sys1, SystemData* sys2);
#endif // ES_APP_COLLECTION_SYSTEM_MANAGER_H
