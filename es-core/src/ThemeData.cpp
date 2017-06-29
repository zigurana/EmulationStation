#include "ThemeData.h"
#include "Renderer.h"
#include "resources/Font.h"
#include "Sound.h"
#include "resources/TextureResource.h"
#include "Log.h"
#include "Settings.h"
#include "pugixml/src/pugixml.hpp"
#include <boost/assign.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "../es-app/src/FileData.h"

#include "components/DateTimeComponent.h"
#include "components/ImageComponent.h"
#include "components/NinePatchComponent.h"
#include "../es-app/src/components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "components/ScrollableTextComponent.h"
#include "components/TextComponent.h"
#include "../es-app/src/components/TextListComponent.h"
#include "components/VideoComponent.h"
#ifdef _RPI_
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"


// This is a work around for some ambiguity that is introduced in C++11 that boost::assign::map_list_of leave open.
// We use makeMap(actualmap) to implicitly convert the boost::assign::map_list_of's return type to ElementMapType.
// Problem exists with gcc 4.7 and Boost 1.51.  Works fine with MSVC2010 without this hack.
typedef std::map<std::string, ThemeData::ElementPropertyType> ElementMapType;
template<typename T>
ElementMapType makeMap(const T& mapInit)
{
	ElementMapType m = mapInit;
	return m;
}

//std::vector<std::string> ThemeData::sSupportedViews = boost::assign::list_of("system")("basic")("detailed")("video");
std::vector<std::string> ThemeData::sSupportedFeatures = boost::assign::list_of("video")("carousel")("z-index");

std::map< std::string, ElementMapType > ThemeData::sElementMap = boost::assign::map_list_of
	("image", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("maxSize", NORMALIZED_PAIR)
		("origin", NORMALIZED_PAIR)
		("path", PATH)
		("tile", BOOLEAN)
		("color", COLOR)
		("zIndex", FLOAT)))
	("text", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("text", STRING)
		("backgroundColor", COLOR)
		("fontPath", PATH)
		("fontSize", FLOAT)
		("color", COLOR)
		("alignment", STRING)
		("forceUppercase", BOOLEAN)
		("lineSpacing", FLOAT)
		("value", STRING)
		("zIndex", FLOAT)))
	("textlist", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("selectorHeight", FLOAT)
		("selectorOffsetY", FLOAT)
		("selectorColor", COLOR)
		("selectorImagePath", PATH)
		("selectorImageTile", BOOLEAN)
		("selectedColor", COLOR)
		("primaryColor", COLOR)
		("secondaryColor", COLOR)
		("fontPath", PATH)
		("fontSize", FLOAT)
		("scrollSound", PATH)
		("alignment", STRING)
		("horizontalMargin", FLOAT)
		("forceUppercase", BOOLEAN)
		("lineSpacing", FLOAT)
		("zIndex", FLOAT)))
	("container", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("zIndex", FLOAT)))
	("ninepatch", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("path", PATH)
		("zIndex", FLOAT)))
	("datetime", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("color", COLOR)
		("fontPath", PATH)
		("fontSize", FLOAT)
		("forceUppercase", BOOLEAN)
		("zIndex", FLOAT)))
	("rating", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("color", COLOR)
		("filledPath", PATH)
		("unfilledPath", PATH)
		("zIndex", FLOAT)))
	("sound", makeMap(boost::assign::map_list_of
		("path", PATH)))
	("helpsystem", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("textColor", COLOR)
		("iconColor", COLOR)
		("fontPath", PATH)
		("fontSize", FLOAT)))
	("video", makeMap(boost::assign::map_list_of
		("pos", NORMALIZED_PAIR)
		("size", NORMALIZED_PAIR)
		("maxSize", NORMALIZED_PAIR)
		("origin", NORMALIZED_PAIR)
		("default", PATH)
		("delay", FLOAT)
		("zIndex", FLOAT)
		("showSnapshotNoVideo", BOOLEAN)
		("showSnapshotDelay", BOOLEAN)))
	("carousel", makeMap(boost::assign::map_list_of
		("type", STRING)
		("size", NORMALIZED_PAIR)
		("pos", NORMALIZED_PAIR)
		("color", COLOR)
		("logoScale", FLOAT)
		("logoSize", NORMALIZED_PAIR)
		("maxLogoCount", FLOAT)
		("zIndex", FLOAT)));

namespace fs = boost::filesystem;

#define MINIMUM_THEME_FORMAT_VERSION 3
#define CURRENT_THEME_FORMAT_VERSION 5

// helper
unsigned int getHexColor(const char* str)
{
	ThemeException error;
	if(!str)
		throw error << "Empty color";

	size_t len = strlen(str);
	if(len != 6 && len != 8)
		throw error << "Invalid color (bad length, \"" << str << "\" - must be 6 or 8)";

	unsigned int val;
	std::stringstream ss;
	ss << str;
	ss >> std::hex >> val;

	if(len == 6)
		val = (val << 8) | 0xFF;

	return val;
}

// helper
std::string resolvePath(const char* in, const fs::path& relative)
{
	if(!in || in[0] == '\0')
		return in;

	fs::path relPath = relative.parent_path();
	
	boost::filesystem::path path(in);
	
	// we use boost filesystem here instead of just string checks because 
	// some directories could theoretically start with ~ or .
	if(*path.begin() == "~")
	{
		path = getHomePath() + (in + 1);
	}else if(*path.begin() == ".")
	{
		path = relPath / (in + 1);
	}

	return path.generic_string();
}

std::map<std::string, std::string> mVariables;

std::string &format_variables(const boost::xpressive::smatch &what)
{
	return mVariables[what[1].str()];
}

std::string resolvePlaceholders(const char* in)
{
	if(!in || in[0] == '\0')
		return std::string(in);
		
	std::string inStr(in);
	
	using namespace boost::xpressive;
	sregex rex = "${" >> (s1 = +('.' | _w)) >> '}';
    
	std::string output = regex_replace(inStr, rex, format_variables);

	return output;
}

ThemeData::ThemeData()
{
	mVersion = 0;
}

void ThemeData::loadFile(std::map<std::string, std::string> sysDataMap, const std::string& path)
{
	mPaths.push_back(path);

	ThemeException error;
	error.setFiles(mPaths);

	if(!fs::exists(path))
		throw error << "File does not exist!";

	mVersion = 0;
	mViews.clear();
	mVariables.clear();

	mVariables.insert(sysDataMap.begin(), sysDataMap.end());

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());
	if(!res)
		throw error << "XML parsing error: \n    " << res.description();

	pugi::xml_node root = doc.child("theme");
	if(!root)
		throw error << "Missing <theme> tag!";

	// parse version
	mVersion = root.child("formatVersion").text().as_float(-404);
	if(mVersion == -404)
		throw error << "<formatVersion> tag missing!\n   It's either out of date or you need to add <formatVersion>" << CURRENT_THEME_FORMAT_VERSION << "</formatVersion> inside your <theme> tag.";

	if(mVersion < MINIMUM_THEME_FORMAT_VERSION)
		throw error << "Theme uses format version " << mVersion << ". Minimum supported version is " << MINIMUM_THEME_FORMAT_VERSION << ".";

	parseVariables(root);
	parseIncludes(root);
	parseViews(root);
	parseFeatures(root);
}

void ThemeData::parseIncludes(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);

	for(pugi::xml_node node = root.child("include"); node; node = node.next_sibling("include"))
	{
		const char* relPath = node.text().get();
		std::string path = resolvePath(relPath, mPaths.back());
		if(!ResourceManager::getInstance()->fileExists(path))
			throw error << "Included file \"" << relPath << "\" not found! (resolved to \"" << path << "\")";

		error << "    from included file \"" << relPath << "\":\n    ";

		mPaths.push_back(path);

		pugi::xml_document includeDoc;
		pugi::xml_parse_result result = includeDoc.load_file(path.c_str());
		if(!result)
			throw error << "Error parsing file: \n    " << result.description();

		pugi::xml_node root = includeDoc.child("theme");
		if(!root)
			throw error << "Missing <theme> tag!";

		parseVariables(root);
		parseIncludes(root);
		parseViews(root);
		parseFeatures(root);

		mPaths.pop_back();
	}
}

void ThemeData::parseFeatures(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);

	for(pugi::xml_node node = root.child("feature"); node; node = node.next_sibling("feature"))
	{
		if(!node.attribute("supported"))
			throw error << "Feature missing \"supported\" attribute!";

		const std::string supportedAttr = node.attribute("supported").as_string();

		if (std::find(sSupportedFeatures.begin(), sSupportedFeatures.end(), supportedAttr) != sSupportedFeatures.end())
		{
			parseViews(node);
		}
	}
}

void ThemeData::parseVariables(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);
    
	pugi::xml_node variables = root.child("variables");

	if(!variables)
		return;
    
	for(pugi::xml_node_iterator it = variables.begin(); it != variables.end(); ++it)
	{
		std::string key = it->name();
		std::string val = it->text().as_string();

		if (!val.empty())
			mVariables.insert(std::pair<std::string, std::string>(key, val));
	}
}

void ThemeData::parseViews(const pugi::xml_node& root)
{
	ThemeException error;
	error.setFiles(mPaths);

	// parse views
	for(pugi::xml_node node = root.child("view"); node; node = node.next_sibling("view"))
	{
		if(!node.attribute("name"))
			throw error << "View missing \"name\" attribute!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		std::string viewKey;
		while(off != std::string::npos || prevOff != std::string::npos)
		{
			viewKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);
			
			//if (std::find(sSupportedViews.begin(), sSupportedViews.end(), viewKey) != sSupportedViews.end())
			//{
				ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView())).first->second;
				parseView(node, view);
			//}
		}
	}
}

void ThemeData::parseView(const pugi::xml_node& root, ThemeView& view)
{
	ThemeException error;
	error.setFiles(mPaths);

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		if(!node.attribute("name"))
			throw error << "Element of type \"" << node.name() << "\" missing \"name\" attribute!";

		auto elemTypeIt = sElementMap.find(node.name());
		if(elemTypeIt == sElementMap.end())
			throw error << "Unknown element of type \"" << node.name() << "\"!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off =  nameAttr.find_first_of(delim, prevOff);
		while(off != std::string::npos || prevOff != std::string::npos)
		{
			std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);
			
			parseElement(node, elemTypeIt->second, 
				view.elements.insert(std::pair<std::string, ThemeElement>(elemKey, ThemeElement())).first->second);

			if(std::find(view.orderedKeys.begin(), view.orderedKeys.end(), elemKey) == view.orderedKeys.end())
				view.orderedKeys.push_back(elemKey);
		}
	}
}


void ThemeData::parseElement(const pugi::xml_node& root, const std::map<std::string, ElementPropertyType>& typeMap, ThemeElement& element)
{
	ThemeException error;
	error.setFiles(mPaths);

	element.type = root.name();
	element.extra = root.attribute("extra").as_bool(false);
	
	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		auto typeIt = typeMap.find(node.name());
		if(typeIt == typeMap.end())
			throw error << "Unknown property type \"" << node.name() << "\" (for element of type " << root.name() << ").";

		std::string str = resolvePlaceholders(node.text().as_string());

		switch(typeIt->second)
		{
		case NORMALIZED_PAIR:
		{
			size_t divider = str.find(' ');
			if(divider == std::string::npos) 
				throw error << "invalid normalized pair (property \"" << node.name() << "\", value \"" << str.c_str() << "\")";

			std::string first = str.substr(0, divider);
			std::string second = str.substr(divider, std::string::npos);

			Eigen::Vector2f val(atof(first.c_str()), atof(second.c_str()));

			element.properties[node.name()] = val;
			break;
		}
		case STRING:
			element.properties[node.name()] = str;
			break;
		case PATH:
		{
			std::string path = resolvePath(str.c_str(), mPaths.back().string());
			if(!ResourceManager::getInstance()->fileExists(path))
			{
				std::stringstream ss;
				ss << "  Warning " << error.msg; // "from theme yadda yadda, included file yadda yadda
				ss << "could not find file \"" << node.text().get() << "\" ";
				if(node.text().get() != path)
					ss << "(which resolved to \"" << path << "\") ";
				LOG(LogWarning) << ss.str();
			}
			element.properties[node.name()] = path;
			break;
		}
		case COLOR:
			element.properties[node.name()] = getHexColor(str.c_str());
			break;
		case FLOAT:
		{
			float floatVal = static_cast<float>(strtod(str.c_str(), 0));
			element.properties[node.name()] = floatVal;
			break;
		}

		case BOOLEAN:
		{
			// only look at first char
			char first = str[0];
			// 1*, t* (true), T* (True), y* (yes), Y* (YES)
			bool boolVal = (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');

			element.properties[node.name()] = boolVal;
			break;
		}
		default:
			throw error << "Unknown ElementPropertyType for \"" << root.attribute("name").as_string() << "\", property " << node.name();
		}
	}
}

bool ThemeData::hasView(const std::string& view)
{
	auto viewIt = mViews.find(view);
	return (viewIt != mViews.end());
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto viewIt = mViews.find(view);
	if(viewIt == mViews.end())
		return NULL; // not found

	auto elemIt = viewIt->second.elements.find(element);
	if(elemIt == viewIt->second.elements.end()) return NULL;

	if(elemIt->second.type != expectedType && !expectedType.empty())
	{
		LOG(LogWarning) << " requested mismatched theme type for [" << view << "." << element << "] - expected \"" 
			<< expectedType << "\", got \"" << elemIt->second.type << "\"";
		return NULL;
	}

	return &elemIt->second;
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> theme = nullptr;
	if(theme == nullptr)
	{
		theme = std::shared_ptr<ThemeData>(new ThemeData());

		const std::string path = getHomePath() + "/.emulationstation/es_theme_default.xml";
		if(fs::exists(path))
		{
			try
			{
				std::map<std::string, std::string> emptyMap;
				theme->loadFile(emptyMap, path);
			} catch(ThemeException& e)
			{
				LOG(LogError) << e.what();
				theme = std::shared_ptr<ThemeData>(new ThemeData()); //reset to empty
			}
		}
	}

	return theme;
}

std::vector<GuiComponent*> ThemeData::makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	std::vector<GuiComponent*> comps;

	auto viewIt = theme->mViews.find(view);
	if(viewIt == theme->mViews.end())
		return comps;
	
	for(auto it = viewIt->second.orderedKeys.begin(); it != viewIt->second.orderedKeys.end(); it++)
	{
		ThemeElement& elem = viewIt->second.elements.at(*it);
		if(elem.extra)
		{
			GuiComponent* comp = NULL;
			const std::string& t = elem.type;
			if(t == "image")
				comp = new ImageComponent(window);
			else if(t == "text")
				comp = new TextComponent(window);

			comp->setDefaultZIndex(10);
			comp->applyTheme(theme, view, *it, ThemeFlags::ALL);
			comps.push_back(comp);
		}
	}

	return comps;
}

std::map<std::string,GuiComponent*> ThemeData::makeThemeComponents(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	LOG(LogDebug) << "ThemeData::makeThemeComponents( view = " << view << " )";

	std::map<std::string,GuiComponent*> comps;

	auto viewIt = theme->mViews.find(view);
	if (viewIt == theme->mViews.end())
		return comps;

	for (auto it = viewIt->second.orderedKeys.begin(); it != viewIt->second.orderedKeys.end(); it++)
	{
		ThemeElement& elem = viewIt->second.elements.at(*it);
		GuiComponent* comp = NULL;
		const std::string& t = elem.type;
		std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
		
		if (t == "container")
		{
			comp = new ScrollableContainer(window);
			comp->setDefaultZIndex(10);
		}
		else if (t == "datetime")
		{
			comp = new DateTimeComponent(window, DateTimeComponent::DISP_RELATIVE_TO_NOW);
			// TODO: reconsider, this is probably wrong (i.e. for date of release and such)
			comp->setDefaultZIndex(40);
		}
		else if (t == "image")
		{
			comp = new ImageComponent(window);
			comp->setDefaultZIndex(30);
		}
		else if (t == "ninepatch")
		{
			comp = new NinePatchComponent(window);
			comp->setDefaultZIndex(35);
		}
		else if (t == "rating")
		{
			comp = new RatingComponent(window);
			comp->setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());
			comp->setDefaultZIndex(40);
		}
		else if (t == "text")
		{
			if (*it == "md_description")
			{
				comp = new ScrollableTextComponent(window);
			}
			else
			{
				comp = new TextComponent(window);
			}
			comp->setDefaultZIndex(50.0);
		}
		else if (t == "textlist")
		{
			comp = new TextListComponent<FileData*>(window);
			comp->setDefaultZIndex(50);
		}
		else if (t == "video")
		{
			// TODO: in videoGameListView, mVideo is derived to be a specific implementation of the abstract base class VideoComponent,
			// either the VLC based or the OMX based variant. We would like to have a generic constructor for the suitable type, and not
			// care about this at all. Untill then, we have this (and the ugly inclusions at top):
#ifdef _RPI_
			if (Settings::getInstance()->getBool("VideoOmxPlayer"))
				comp = new VideoPlayerComponent(window, std::string(""));
			else
				comp = new VideoVlcComponent(window, std::string(""));
#else
			comp = new VideoVlcComponent(window, std::string(""));
#endif
			comp->setDefaultZIndex(30);
		}
		else
		{
			// not a supported GuiComponent type: Carousel (TODO)
			LOG(LogDebug) << "    Theme element of type '" << t << "' is not recognized (yet), skipping.";
		}

		if (comp)
		{
			comps.insert(std::pair < std::string, GuiComponent* >(*it, comp));
		}
	}

	return comps;
}

std::map<std::string, ThemeSet> ThemeData::getThemeSets()
{
	std::map<std::string, ThemeSet> sets;

	static const size_t pathCount = 2;
	fs::path paths[pathCount] = { 
		"/etc/emulationstation/themes", 
		getHomePath() + "/.emulationstation/themes" 
	};

	fs::directory_iterator end;

	for(size_t i = 0; i < pathCount; i++)
	{
		if(!fs::is_directory(paths[i]))
			continue;

		for(fs::directory_iterator it(paths[i]); it != end; ++it)
		{
			if(fs::is_directory(*it))
			{
				ThemeSet set = {*it};
				sets[set.getName()] = set;
			}
		}
	}

	return sets;
}

fs::path ThemeData::getThemeFromCurrentSet(const std::string& system)
{
	auto themeSets = ThemeData::getThemeSets();
	if(themeSets.empty())
	{
		// no theme sets available
		return "";
	}

	auto set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.end())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.begin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	return set->second.getThemePath(system);
}

std::vector<std::string> ThemeData::getViewNames()
{
	std::vector<std::string> list;
	for (auto it = mViews.begin(); it != mViews.end(); it++)
	{
		list.push_back(it->first);
	}
	return list;
}

std::vector<std::string> ThemeData::getGameListViewNames()
{
	std::vector<std::string> list = getViewNames();
	list.erase(std::remove(list.begin(), list.end(), "system"), list.end());
	list.erase(std::remove(list.begin(), list.end(), "grid"), list.end());
	return list;
}