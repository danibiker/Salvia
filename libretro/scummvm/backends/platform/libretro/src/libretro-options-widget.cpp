/* Copyright (C) 2024 Giovanni Cascione <ing.cascione@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#define FORBIDDEN_SYMBOL_ALLOW_ALL
#include <streams/file_stream.h>
#include "backends/platform/libretro/include/libretro-options-widget.h"
#include "backends/platform/libretro/include/libretro-core.h"
#include "backends/platform/libretro/include/libretro-fs.h"
#include "backends/platform/libretro/include/libretro-os.h"
#include "gui/launcher.h"
#include "common/translation.h"
#include "common/formats/json.h"
#include "common/algorithm.h"

enum {
	kPlaylistPathCmd = 'pchp',
	kPlaylistPathClearCmd = 'pclp',
	kPlaylistGenerateCmd = 'pgen'
};

enum {
	kPlaylistFormatJSON = 0,
	kPlaylistFormat6lines,
};

enum {
	kHooksLocationSave = 0,
	kHooksLocationGame,
};

LibretroOptionsWidget::LibretroOptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain) :
	OptionsContainerWidget(boss, name, "LibretroOptionsDialog", domain) {

	new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistHeader", _("LIBRETRO PLAYLIST GENERATOR"));
	new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistSubheader", _("(check '? > Libretro playlist' for detailed info)"));
	(new GUI::ButtonWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistPathButton", _("Playlists Path"), Common::U32String(), kPlaylistPathCmd))->setTarget(this);
	_playlistPath = new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistPath", _("Specifies where your playlist will be saved."));
	GUI::addClearButton(widgetsBoss(), "LibretroOptionsDialog.PlaylistPathButtonClear", kPlaylistPathClearCmd);

	new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistVersionText", _("Playlist format:"));
	_playlistVersion = new GUI::PopUpWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistVersion");
	_playlistVersion->appendEntry(Common::U32String("JSON "), kPlaylistFormatJSON);
	_playlistVersion->appendEntry(Common::U32String("6-lines"), kPlaylistFormat6lines);

	new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.HooksLocationText", _("Hooks location:"));
	_hooksLocation = new GUI::PopUpWidget(widgetsBoss(), "LibretroOptionsDialog.HooksLocation");
	_hooksLocation->appendEntry(_("All in save folder"), kHooksLocationSave);
	_hooksLocation->appendEntry(_("One in each game folder"), kHooksLocationGame);

	_hooksClear = new GUI::CheckboxWidget(widgetsBoss(), "LibretroOptionsDialog.HooksClear", _("Clear existing hooks"));

	(new GUI::ButtonWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistGenerateButton", _("Generate playlist"), Common::U32String(), kPlaylistGenerateCmd))->setTarget(this);

	new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistStatusText", _("Status: "));
	_playlistStatus = new GUI::StaticTextWidget(widgetsBoss(), "LibretroOptionsDialog.PlaylistStatus", Common::String("-"));
}

LibretroOptionsWidget::~LibretroOptionsWidget() {
}

void LibretroOptionsWidget::defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName, const Common::String &overlayedLayout) const {
	layouts.addDialog(layoutName, overlayedLayout)
	.addLayout(GUI::ThemeLayout::kLayoutVertical, 8)
	.addPadding(0, 0, 0, 0)
	.addWidget("PlaylistHeader", "", -1, layouts.getVar("Globals.Line.Height"))
	.addWidget("PlaylistSubheader", "", -1, layouts.getVar("Globals.Line.Height"))
	.addLayout(GUI::ThemeLayout::kLayoutHorizontal, 10, GUI::ThemeLayout::kItemAlignCenter)
	.addPadding(0, 0, 0, 0)
	.addWidget("PlaylistPathButton", "Button")
	.addWidget("PlaylistPath", "", -1, layouts.getVar("Globals.Line.Height"))
	.addWidget("PlaylistPathButtonClear", "SearchClearButton", layouts.getVar("Globals.Line.Height"), layouts.getVar("Globals.Line.Height"))
	.closeLayout()

	.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
	.addPadding(0, 0, 0, 0)
	.addWidget("PlaylistVersionText", "OptionsLabel")
	.addWidget("PlaylistVersion", "PopUp")
	.closeLayout()

	.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
	.addPadding(0, 0, 0, 0)
	.addWidget("HooksLocationText", "OptionsLabel")
	.addWidget("HooksLocation", "PopUp")
	.closeLayout()

	.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
	.addPadding(0, 0, 0, 0)
	.addWidget("HooksClear", "Checkbox")
	.closeLayout()

	.addLayout(GUI::ThemeLayout::kLayoutHorizontal)
	.addPadding(0, 0, 0, 0)
	.addWidget("PlaylistGenerateButton", "WideButton")
	.addSpace(layouts.getVar("Globals.Line.Height") * 2)
	.addWidget("PlaylistStatusText", "", -1, layouts.getVar("Globals.Line.Height"))
	.addWidget("PlaylistStatus", "", -1, layouts.getVar("Globals.Line.Height"))
	.closeLayout()

	.closeLayout()
	.closeDialog();
}

bool LibretroOptionsWidget::cleanFolder(Common::String &path) {
	bool res = true;
	Common::ArchiveMemberList listHooks;
	Common::FSNode(Common::Path(path)).listChildren(listHooks, "*." CORE_EXTENSIONS);
	for (auto rfIt119 = listHooks.begin(), rfEnd119 = listHooks.end(); rfIt119 != rfEnd119; ++rfIt119) {
		Common::ArchiveMemberPtr hook = *rfIt119;
		Common::FSNode *fshook = dynamic_cast<Common::FSNode *>(hook.get());
		if (!fshook || fshook->isDirectory())
			continue;
		if (remove(fshook->getPath().toString().c_str()) == 0)
			if (retro_log_cb)
				retro_log_cb(RETRO_LOG_INFO, "Hook file deleted in '%s'.\n", fshook->getPath().toString().c_str());
		else {
			res = false;
			if (retro_log_cb)
				retro_log_cb(RETRO_LOG_WARN, "Failed to delete hook file in '%s'.\n", fshook->getPath().toString().c_str());
		}
	}
	return res;
}

/* Game list XML for the frontend ------------------------------------------
 * Written to the system directory each time the playlist/hooks are
 * generated, with one <game> per hook file. The name attribute is the hook
 * file name without extension (the ScummVM target, e.g. "monkey-es"), so the
 * frontend can show the full description instead of the target. Same layout
 * as the MAME/FBNeo style lists used by Salvia (assets/extra/merged_fbneo.xml). */
#define LIBRETRO_GAMELIST_XML "merged_scummvm.xml"

struct LibretroGameListEntry {
	Common::String target;
	Common::String description;
	Common::String engineId;
	Common::String gameId;
	Common::String language;
	Common::String platform;
};

struct LibretroGameListEntryLess {
	bool operator()(const LibretroGameListEntry &a, const LibretroGameListEntry &b) const {
		return a.target.compareToIgnoreCase(b.target) < 0;
	}
};

static Common::String libretroXmlEscape(const Common::String &in) {
	Common::String out;
	for (uint i = 0; i < in.size(); ++i) {
		const char c = in[i];
		switch (c) {
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': out += "&quot;"; break;
		case '\'': out += "&apos;"; break;
		default:
			/* Drop other control characters, invalid in XML 1.0 */
			if ((unsigned char)c >= 0x20 || c == '\t')
				out += c;
			break;
		}
	}
	return out;
}

static void libretroXmlAttr(Common::String &line, const char *name, const Common::String &value) {
	if (value.empty())
		return;
	line += Common::String::format(" %s=\"", name);
	line += libretroXmlEscape(value);
	line += "\"";
}

static bool libretroWriteGameListXml(Common::Array<LibretroGameListEntry> &entries) {
	const char *systemDir = retro_get_system_dir();
	if (!systemDir || !*systemDir) {
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_WARN, "No system directory, game list XML not written.\n");
		return false;
	}

	Common::String xmlPath = LibRetroFilesystemNode(Common::String(systemDir)).getPath();
	if (xmlPath.empty() || xmlPath.lastChar() != '/')
		xmlPath += "/";
	xmlPath += LIBRETRO_GAMELIST_XML;

	RFILE *xmlFile = filestream_open(xmlPath.c_str(), RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!xmlFile) {
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_ERROR, "Failed to create game list XML at '%s'.\n", xmlPath.c_str());
		return false;
	}

	Common::sort(entries.begin(), entries.end(), LibretroGameListEntryLess());

	Common::String out;
	out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	out += "<!DOCTYPE mame [\n";
	out += "\t<!ELEMENT mame (game+)>\n";
	out += "\t\t<!ELEMENT game (description, year?, manufacturer?)>\n";
	out += "\t\t\t<!ATTLIST game name CDATA #REQUIRED>\n";
	out += "\t\t\t<!ATTLIST game engine CDATA #IMPLIED>\n";
	out += "\t\t\t<!ATTLIST game gameid CDATA #IMPLIED>\n";
	out += "\t\t\t<!ATTLIST game language CDATA #IMPLIED>\n";
	out += "\t\t\t<!ATTLIST game platform CDATA #IMPLIED>\n";
	out += "\t\t<!ELEMENT description (#PCDATA)>\n";
	out += "\t\t<!ELEMENT year (#PCDATA)>\n";
	out += "\t\t<!ELEMENT manufacturer (#PCDATA)>\n";
	out += "]>\n";
	out += "<mame>\n";

	for (uint i = 0; i < entries.size(); ++i) {
		const LibretroGameListEntry &e = entries[i];
		Common::String line("\t<game");
		libretroXmlAttr(line, "name", e.target);
		libretroXmlAttr(line, "engine", e.engineId);
		libretroXmlAttr(line, "gameid", e.gameId);
		libretroXmlAttr(line, "language", e.language);
		libretroXmlAttr(line, "platform", e.platform);
		line += ">\n\t\t<description>";
		line += libretroXmlEscape(e.description);
		line += "</description>\n\t</game>\n";
		out += line;
	}
	out += "</mame>\n";

	const bool ok = filestream_write(xmlFile, out.c_str(), out.size()) == (int64_t)out.size();
	filestream_close(xmlFile);

	if (retro_log_cb)
		retro_log_cb(ok ? RETRO_LOG_INFO : RETRO_LOG_ERROR, "%s game list XML with %u entries at '%s'.\n",
		             ok ? "Wrote" : "Failed to write", entries.size(), xmlPath.c_str());
	return ok;
}

bool LibretroOptionsWidget::generatePlaylist(Common::String playlistPath) {
	bool isFirstEntry = true;
	bool success = true;
	bool cleanSuccess = true;
	Common::String playlistElement;
	Common::String playlistFooter;
	Common::String path;
	char separator[2] = {0};
	Common::String hookPath;
	Common::String hookFilePath;
	Common::String title;

	Common::JSONObject root;
	Common::JSONArray  items;
	Common::Array<LibretroGameListEntry> gameList;

	/* Create playlist file */
	RFILE *playlistFile = filestream_open(Common::String(playlistPath + "/" + CORE_NAME + ".lpl").c_str(), RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!playlistFile) {
		_playlistStatus->setLabel(_("Failed, can't access playlist file"));
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_ERROR, "Failed to access playlst file at '%s'.\n", Common::String(playlistPath +  "/" + CORE_NAME + ".lpl").c_str());
		return false;
	} else
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_INFO, "Playlist file created in '%s'.\n", Common::String(playlistPath + CORE_NAME + ".lpl").c_str());

	/* Create common hook folder */
	if (ConfMan.getInt("libretro_hooks_location", _domain) != kHooksLocationGame) {
		hookPath = LibRetroFilesystemNode(Common::String(LIBRETRO_G_SYSTEM->getSaveDir())).getPath() + "/" + COMMON_HOOKS_FOLDER;
		LibRetroFilesystemNode(hookPath).createDirectory();
		if (ConfMan.getBool("libretro_hooks_clear", _domain)) {
			cleanSuccess = cleanFolder(hookPath);
		}
	}

	/* Setup playlist template */
	if (ConfMan.getInt("libretro_playlist_version", _domain) != kPlaylistFormat6lines) {
		const char *cCorePath = retro_get_core_dir();
		LibRetroFilesystemNode corePath(".");
		if (cCorePath)
			corePath = LibRetroFilesystemNode(Common::String(cCorePath));

		root["version"]			= new Common::JSONValue(Common::String("1.5"));
		root["default_core_path"]  = new Common::JSONValue(corePath.exists() ? corePath.getPath() : Common::String(""));
		root["default_core_name"]  = new Common::JSONValue(Common::String("ScummVM"));
		root["label_display_mode"] = new Common::JSONValue((long long)0);
		root["right_thumbnail_mode"] = new Common::JSONValue((long long)2);
		root["left_thumbnail_mode"]  = new Common::JSONValue((long long)3);
		root["sort_mode"]			= new Common::JSONValue((long long)0);
	} else
		playlistElement = "%s%s\n%s\nDETECT\nDETECT\nDETECT\nScummVM.lpl\n";

	/* Crawl ScummVM game list */
	Common::ConfigManager::DomainMap::iterator iter = ConfMan.beginGameDomains();
	for (; iter != ConfMan.endGameDomains(); ++iter) {

		if (!iter->_value.tryGetVal("path", path) || iter->_value.contains("id_came_from_command_line"))
			continue;

		Common::Path cleanPath = Common::Path::fromConfig(path);
		if (!Common::FSNode(cleanPath).isDirectory())
			continue;

		if (ConfMan.getInt("libretro_hooks_location", _domain) == kHooksLocationGame) {
			hookPath = LibRetroFilesystemNode(cleanPath.toString()).getPath();
			if (ConfMan.getBool("libretro_hooks_clear", _domain))
				if (!cleanFolder(hookPath)) cleanSuccess = false;
		}

		title = iter->_key;
		iter->_value.tryGetVal("description", title);
		hookFilePath = hookPath + "/" + iter->_key.c_str() + "." + CORE_EXTENSIONS;

		if (ConfMan.getInt("libretro_playlist_version", _domain) != kPlaylistFormat6lines) {
			Common::JSONObject item;
			item["path"]	  = new Common::JSONValue(hookFilePath);
			item["label"]	 = new Common::JSONValue(title);
			item["core_path"] = new Common::JSONValue(Common::String("DETECT"));
			item["core_name"] = new Common::JSONValue(Common::String("DETECT"));
			item["crc32"]	 = new Common::JSONValue(Common::String("DETECT"));
			item["db_name"]   = new Common::JSONValue(Common::String(CORE_NAME ".lpl"));

			items.push_back(new Common::JSONValue(item));
		} else {
			filestream_printf(playlistFile, playlistElement.c_str(), separator, hookFilePath.c_str(), title.c_str());
			if (isFirstEntry) {
				*separator = ',';
				isFirstEntry = false;
			}
		}

		/* Create hook file */
		RFILE *hookFile = filestream_open(hookFilePath.c_str(), RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
		if (!hookFile) {
			if (retro_log_cb)
				retro_log_cb(RETRO_LOG_ERROR, "Failed to access/create hook file at '%s'.\n", hookFilePath.c_str());
			success = false;
			break;
		} else
			if (retro_log_cb)
				retro_log_cb(RETRO_LOG_INFO, "Hook file created in '%s'.\n", hookFilePath.c_str());


		filestream_printf(hookFile, "%s", iter->_key.c_str());
		filestream_close(hookFile);

		LibretroGameListEntry entry;
		entry.target = iter->_key;
		entry.description = title;
		iter->_value.tryGetVal("engineid", entry.engineId);
		iter->_value.tryGetVal("gameid", entry.gameId);
		iter->_value.tryGetVal("language", entry.language);
		iter->_value.tryGetVal("platform", entry.platform);
		gameList.push_back(entry);
	}

	if (ConfMan.getInt("libretro_playlist_version", _domain) != kPlaylistFormat6lines) {
		root["items"] = new Common::JSONValue(items);
		Common::JSONValue *rootVal = new Common::JSONValue(root);
		Common::String out = Common::JSON::stringify(rootVal);
		filestream_write(playlistFile, out.c_str(), out.size());
		delete rootVal;
	} else {
		filestream_printf(playlistFile, playlistFooter.c_str());
	}
	filestream_close(playlistFile);

	const bool xmlSuccess = libretroWriteGameListXml(gameList);

	Common::String response;
	if (success) {
		response = _("Done");
		if (!cleanSuccess)
			response += " (" +  _("cleaning failed") + ")";
		if (!xmlSuccess)
			response += " (" + _("game list XML failed") + ")";
	} else
		response = _("Failed, can't create hook files");

	_playlistStatus->setLabel(response);

	return success;
}

void LibretroOptionsWidget::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kPlaylistPathCmd: {
		GUI::BrowserDialog browser(_("Select Playlist directory"), true);
		if (browser.runModal() > 0) {
			Common::FSNode dir(browser.getResult());
			_playlistPath->setLabel(dir.getPath().toString());
		}
		break;
	}
	case kPlaylistPathClearCmd: {
		_playlistPath->setLabel(_c("None", "path"));
		break;
	}
	case kPlaylistGenerateCmd: {
		save();
		_playlistStatus->setLabel(Common::String("-"));
		generatePlaylist(ConfMan.get("libretro_playlist_path", _domain));
		break;
	}
	default:
		GUI::OptionsContainerWidget::handleCommand(sender, cmd, data);
	}

}

void LibretroOptionsWidget::load() {
	_playlistPath->setLabel(ConfMan.get("libretro_playlist_path", _domain));

	if (ConfMan.getInt("libretro_playlist_version", _domain) == kPlaylistFormat6lines)
		_playlistVersion->setSelected(kPlaylistFormat6lines);
	else
		_playlistVersion->setSelected(kPlaylistFormatJSON);

	if (ConfMan.getInt("libretro_hooks_location", _domain) == kHooksLocationGame)
		_hooksLocation->setSelected(kHooksLocationGame);
	else
		_hooksLocation->setSelected(kHooksLocationSave);

	_hooksClear->setState(ConfMan.getBool("libretro_hooks_clear", _domain));
}

bool LibretroOptionsWidget::save() {
	Common::String data = _playlistPath->getLabel();
	if (data.empty())
		ConfMan.set("libretro_playlist_path", "", _domain);
	else
		ConfMan.set("libretro_playlist_path", data, _domain);

	if (_playlistVersion->getSelected() == kPlaylistFormat6lines)
		ConfMan.setInt("libretro_playlist_version", kPlaylistFormat6lines, _domain);
	else
		ConfMan.setInt("libretro_playlist_version", kPlaylistFormatJSON, _domain);

	if (_hooksLocation->getSelected() == kHooksLocationGame)
		ConfMan.setInt("libretro_hooks_location", kHooksLocationGame, _domain);
	else
		ConfMan.setInt("libretro_hooks_location", kHooksLocationSave, _domain);

	ConfMan.setBool("libretro_hooks_clear", _hooksClear->getState(), _domain);

	/* Always return true to call applyBackendSettings every time settings are applied */
	return true;
}
