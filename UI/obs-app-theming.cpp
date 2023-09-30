/******************************************************************************
    Copyright (C) 2023 by Dennis Sädtler <dennis@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <util/cf-parser.h>

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDirIterator>
#include <QGuiApplication>

#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "obs-app-theming.hpp"
#include "obs-proxy-style.hpp"
#include "platform.hpp"

#include "ui-config.h"

using namespace std;

struct CFParser {
	cf_parser cfp = {};
	~CFParser() { cf_parser_free(&cfp); }
	operator cf_parser *() { return &cfp; }
	cf_parser *operator->() { return &cfp; }
};

static OBSTheme *ParseThemeMeta(const QString &path)
{
	QFile themeFile(path);
	if (!themeFile.open(QIODeviceBase::ReadOnly))
		return nullptr;

	OBSTheme *meta = nullptr;
	const QByteArray data = themeFile.readAll();
	CFParser cfp;
	int ret;

	if (!cf_parser_parse(cfp, data.constData(), QT_TO_UTF8(path)))
		return nullptr;

	if (cf_token_is(cfp, "OBSThemeMeta") ||
	    cf_go_to_token(cfp, "OBSThemeMeta", nullptr)) {

		if (!cf_next_token(cfp))
			return nullptr;

		if (!cf_token_is(cfp, "{"))
			return nullptr;

		meta = new OBSTheme();

		for (;;) {
			if (!cf_next_token(cfp)) {
				delete meta;
				return nullptr;
			}

			ret = cf_token_is_type(cfp, CFTOKEN_NAME, "name",
					       nullptr);
			if (ret != PARSE_SUCCESS)
				break;

			string name(cfp->cur_token->str.array,
				    cfp->cur_token->str.len);

			ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp)) {
				delete meta;
				return nullptr;
			}

			ret = cf_token_is_type(cfp, CFTOKEN_STRING, "value",
					       ";");

			if (ret != PARSE_SUCCESS)
				continue;

			BPtr str = cf_literal_to_str(cfp->cur_token->str.array,
						     cfp->cur_token->str.len);

			if (str) {
				if (name == "dark")
					meta->isDark = strcmp(str, "true") == 0;
				else if (name == "extends")
					meta->extends = str;
				else if (name == "author")
					meta->author = str;
				else if (name == "id")
					meta->id = str;
				else if (name == "name")
					meta->name = str;
			}

			if (!cf_go_to_token(cfp, ";", nullptr)) {
				delete meta;
				return nullptr;
			}
		}
	}

	if (meta) {
		meta->isBaseTheme = path.endsWith(".obt");

		if (meta->id.isEmpty() || meta->name.isEmpty() ||
		    (!meta->isBaseTheme && meta->extends.isEmpty())) {
			/* Theme is invalid */
			delete meta;
			meta = nullptr;
		} else {
			BPtr absPath = os_get_abs_path_ptr(QT_TO_UTF8(path));
			meta->location = absPath;
			meta->isHighContrast = path.endsWith(".oha");
			meta->isVisible = !path.contains("System");
		}
	}

	return meta;
}

static QColor ParseColour(CFParser &cfp)
{
	const char *array;
	uint32_t color = 0;
	QColor res(QColor::Invalid);

	if (cf_token_is(cfp, "#")) {
		if (!cf_next_token(cfp))
			return res;

		color = strtol(cfp->cur_token->str.array, nullptr, 16);
	} else if (cf_token_is(cfp, "rgb")) {
		int ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10) << 16;

		ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10) << 8;

		ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10);

		ret = cf_next_token_should_be(cfp, ")", ";", nullptr);
		if (ret != PARSE_SUCCESS)
			return res;
	}

	res = color;
	return res;
}

static vector<OBSThemeVariable> ParseThemeVariables(const char *themeData)
{
	CFParser cfp;
	int ret;

	std::vector<OBSThemeVariable> vars;

	if (!cf_parser_parse(cfp, themeData, nullptr))
		return vars;

	if (!cf_token_is(cfp, "OBSThemeVars") &&
	    !cf_go_to_token(cfp, "OBSThemeVars", nullptr))
		return vars;

	if (!cf_next_token(cfp))
		return {};

	if (!cf_token_is(cfp, "{"))
		return {};

	for (;;) {
		if (!cf_next_token(cfp))
			return vars;

		ret = cf_token_is_type(cfp, CFTOKEN_NAME, "key", nullptr);
		if (ret != PARSE_SUCCESS)
			break;

		QString key = QString::fromUtf8(cfp->cur_token->str.array,
						cfp->cur_token->str.len);
		OBSThemeVariable var;
		var.name = key;

		ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
		if (ret != PARSE_SUCCESS)
			continue;

		if (!cf_next_token(cfp))
			return vars;

		if (cfp->cur_token->type == CFTOKEN_NUM) {
			const char *ch = cfp->cur_token->str.array;
			const char *end = ch + cfp->cur_token->str.len;
			double f = os_strtod(ch);

			var.value = f;
			var.type = OBSThemeVariable::Number;

			/* Look for a suffix and mark variable as size if it exists */
			while (ch < end) {
				if (!isdigit(*ch) && !isspace(*ch)) {
					var.suffix =
						QString::fromUtf8(ch, end - ch);
					var.type = OBSThemeVariable::Size;
					break;
				}
				ch++;
			}
		} else if (cf_token_is(cfp, "rgb") || cf_token_is(cfp, "#")) {
			QColor color = ParseColour(cfp);
			if (!color.isValid())
				continue;

			var.value = color;
			var.type = OBSThemeVariable::Color;
		} else if (cf_token_is(cfp, "alias")) {
			ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;
			if (!cf_next_token(cfp))
				return vars;

			var.value = QString::fromUtf8(cfp->cur_token->str.array,
						      cfp->cur_token->str.len);
			var.type = OBSThemeVariable::Alias;

			ret = cf_next_token_should_be(cfp, ")", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;
		} else {
			var.type = OBSThemeVariable::String;
			BPtr strVal =
				cf_literal_to_str(cfp->cur_token->str.array,
						  cfp->cur_token->str.len);
			var.value = QString::fromUtf8(strVal.Get());
		}

		if (!cf_next_token(cfp))
			return vars;

		if (cf_token_is(cfp, "!") &&
		    cf_next_token_should_be(cfp, "editable", nullptr,
					    nullptr) == PARSE_SUCCESS) {
			var.editable = true;
		}

		vars.push_back(std::move(var));

		if (!cf_token_is(cfp, ";") &&
		    !cf_go_to_token(cfp, ";", nullptr))
			return vars;
	}

	return vars;
}

void OBSApp::ParseThemeCustomisation(QHash<QString, OBSThemeVariable> &vars)
{
	if (!config_get_bool(globalConfig, "Appearance", "Customisation"))
		return;

	QPalette pal = palette();
	QString sectionName = "Appearance.Customisation." + currentTheme->id;

	for (const QString &key : vars.keys()) {
		if (!config_has_user_value(globalConfig,
					   QT_TO_UTF8(sectionName),
					   QT_TO_UTF8(key)))
			continue;

		const char *str;
		double dbl;
		OBSThemeVariable &var = vars[key];

		switch (var.type) {
		case OBSThemeVariable::Alias:
			continue;
		case OBSThemeVariable::Size:
		case OBSThemeVariable::Number:
			dbl = config_get_double(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = dbl;
			break;
		case OBSThemeVariable::Color:
			str = config_get_string(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = QColor(str);
			break;
		case OBSThemeVariable::String:
			str = config_get_string(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = str;
			break;
		}

		userVariables[key] = var;
	}
}

void OBSApp::FindThemes()
{
	string themeDir;
	themeDir.resize(512);

	QStringList filters;
	filters << "*.obt" // OBS Base Theme
		<< "*.ovt" // OBS Variant Theme
		<< "*.oha" // OBS High-contrast Adjustment layer
		;

	if (GetConfigPath(themeDir.data(), themeDir.capacity(),
			  "obs-studio/themes/") > 0) {
		QDirIterator it(QT_UTF8(themeDir.c_str()), filters,
				QDir::Files);

		while (it.hasNext()) {
			OBSTheme *theme = ParseThemeMeta(it.next());
			if (theme && !themes.contains(theme->id))
				themes[theme->id] = std::move(*theme);
			else
				delete theme;
		}
	}

	GetDataFilePath("themes/", themeDir);
	QDirIterator it(QString::fromStdString(themeDir), filters, QDir::Files);
	while (it.hasNext()) {
		OBSTheme *theme = ParseThemeMeta(it.next());
		if (theme && !themes.contains(theme->id))
			themes[theme->id] = std::move(*theme);
		else
			delete theme;
	}

	/* Build dependency tree for all themes, removing ones that have items missing. */
	QSet<QString> invalid;

	for (OBSTheme &theme : themes) {
		if (theme.extends.isEmpty()) {
			if (!theme.isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Theme "%s" is not base, but does not specify parent!)",
				     QT_TO_UTF8(theme.id));
				invalid.insert(theme.id);
			}

			continue;
		}

		QString parentId = theme.extends;
		while (!parentId.isEmpty()) {
			OBSTheme *parent = GetTheme(parentId);
			if (!parent) {
				blog(LOG_ERROR,
				     R"(Theme "%s" is missing ancestor "%s"!)",
				     QT_TO_UTF8(theme.id),
				     QT_TO_UTF8(parentId));
				invalid.insert(theme.id);
				break;
			}

			if (theme.isBaseTheme && !parent->isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Ancestor "%s" of base theme "%s" is not a base theme!)",
				     QT_TO_UTF8(parent->id),
				     QT_TO_UTF8(theme.id));
				invalid.insert(theme.id);
				break;
			}

			/* Mark this theme as a variant of first parent that is a base theme. */
			if (!theme.isBaseTheme && parent->isBaseTheme &&
			    theme.parent.isEmpty())
				theme.parent = parent->id;

			theme.dependencies.push_front(parent->id);
			parentId = parent->extends;

			if (parentId.isEmpty() && !parent->isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Final ancestor of "%s" ("%s") is not a base theme!)",
				     QT_TO_UTF8(theme.id),
				     QT_TO_UTF8(parent->id));
				invalid.insert(theme.id);
				break;
			}
		}
	}

	for (const QString &name : invalid) {
		themes.remove(name);
	}
}

static qsizetype FindEndOfOBSMetadata(const QString &content)
{
	/* Find end of last OBS-specific section and strip it, kinda jank but should work */
	qsizetype end = 0;

	for (auto section : {"OBSThemeMeta", "OBSThemeVars", "OBSTheme"}) {
		qsizetype idx = content.indexOf(section, 0);
		if (idx > end) {
			end = content.indexOf('}', idx) + 1;
		}
	}

	return end;
}

static QString PrepareQSS(const QHash<QString, OBSThemeVariable> &vars,
			  const QStringList &contents)
{
	QString stylesheet;
	QString needleTemplate("${%1}");

	for (const QString &content : contents) {
		qsizetype offset = FindEndOfOBSMetadata(content);
		if (offset >= 0) {
			stylesheet += "\n";
			stylesheet += content.sliced(offset);
		}
	}

	for (const OBSThemeVariable &var : vars) {
		const OBSThemeVariable *realVar = &var;
		QString needle = needleTemplate.arg(var.name);

		while (realVar->type == OBSThemeVariable::Alias) {
			QString newKey = var.value.toString();

			if (!vars.contains(newKey)) {
				blog(LOG_ERROR,
				     R"(Variable "%s" (aliased by "%s") does not exist!)",
				     QT_TO_UTF8(newKey), QT_TO_UTF8(var.name));
				realVar = nullptr;
				break;
			}

			const OBSThemeVariable &newVar = vars.value(newKey);
			realVar = &newVar;
		}

		if (!realVar)
			continue;

		QString replace;
		QVariant value = realVar->userValue.isValid()
					 ? realVar->userValue
					 : realVar->value;

		if (realVar->type == OBSThemeVariable::Color) {
			replace = value.value<QColor>().name(QColor::HexRgb);
		} else if (realVar->type == OBSThemeVariable::Size ||
			   realVar->type == OBSThemeVariable::Number) {
			double val = value.toDouble();
			bool isInteger = ceill(val) == val;
			replace = QString::number(val, 'f', isInteger ? 0 : -1);

			if (!realVar->suffix.isEmpty())
				replace += realVar->suffix;
		} else {
			replace = value.toString();
		}

		stylesheet = stylesheet.replace(needle, replace);
	}

	return stylesheet;
}

OBSTheme *OBSApp::GetTheme(const QString &name)
{
	if (!themes.contains(name))
		return nullptr;

	return &themes[name];
}

void OBSApp::themeFileChanged(const QString &path)
{
	themeWatcher->blockSignals(true);
	blog(LOG_INFO, "Theme file \"%s\" changed, reloading...",
	     QT_TO_UTF8(path));
	SetTheme(currentTheme->id);
}

QHash<QString, OBSThemeVariable>
OBSApp::GetThemeVariables(const OBSTheme &theme)
{
	QHash<QString, OBSThemeVariable> vars;
	QStringList themeIds(theme.dependencies);
	themeIds << theme.id;

	for (const QString &themeId : themeIds) {
		blog(LOG_DEBUG, "Processing %s", QT_TO_UTF8(themeId));
		OBSTheme *cur = GetTheme(themeId);

		QFile file(cur->location);
		if (!file.open(QIODeviceBase::ReadOnly))
			return {};

		const QByteArray content = file.readAll();

		for (OBSThemeVariable &var :
		     ParseThemeVariables(content.constData())) {
			vars[var.name] = std::move(var);
		}
	}

	return vars;
}

#define DEBUG_THEME_OUTPUT

bool OBSApp::SetTheme(const QString &name)
{
	OBSTheme *theme = GetTheme(name);
	if (!theme)
		return false;

	setStyleSheet("");
	currentTheme = theme;

	userVariables.clear();
	themeWatcher->removePaths(themeWatcher->files());

	QStringList contents;
	QHash<QString, OBSThemeVariable> vars;
	/* Build list of themes to load (in order) */
	QStringList themeIds(theme->dependencies);
	themeIds << theme->id;

	/* Find and add high contrast adjustment layer if available */
	if (HighContrastEnabled()) {
		for (const OBSTheme &theme_ : themes) {
			if (!theme_.isHighContrast)
				continue;
			if (theme_.parent != theme->id)
				continue;
			themeIds << theme_.id;
			break;
		}
	}

	QStringList filenames;
	for (const QString &themeId : themeIds) {
		OBSTheme *cur = GetTheme(themeId);

		QFile file(cur->location);
		filenames << cur->location;

		if (!file.open(QIODeviceBase::ReadOnly))
			return false;
		const QByteArray content = file.readAll();

		for (OBSThemeVariable &var :
		     ParseThemeVariables(content.constData())) {
			if (var.editable)
				userVariables[var.name] = var;

			vars[var.name] = std::move(var);
		}

		contents.emplaceBack(content.constData());
	}

	ParseThemeCustomisation(vars);
	const QString stylesheet = PrepareQSS(vars, contents);
	setStyleSheet(stylesheet);

#ifdef DEBUG_THEME_OUTPUT
	/* Debug shit remove later */
#ifdef __APPLE__
	string filename("obs-studio/themes/");
	filename += theme->id.toStdString();
	filename += ".out";

	filesystem::path debugOut;
	char configPath[512];
	if (GetConfigPath(configPath, sizeof(configPath), filename.c_str())) {
		debugOut = absolute(filesystem::u8path(configPath));
		filesystem::create_directories(debugOut.parent_path());
	}
#else
	QString debugOut = theme->location + ".out";
#endif
	QFile debugFile(debugOut);
	if (debugFile.open(QIODeviceBase::WriteOnly)) {
		debugFile.write(stylesheet.toUtf8());
		debugFile.flush();
	}
#endif

#ifdef __APPLE__
	SetMacOSDarkMode(theme->isDark);
#endif

	emit StyleChanged();
	themeWatcher->addPaths(filenames);
	QTimer::singleShot(1000, this,
			   [&] { themeWatcher->blockSignals(false); });

	return true;
}

static map<string, string> themeMigrations = {
	{"Yami", DEFAULT_THEME},
	// {"Dark", "com.obsproject.Classic"}, /* Doesn't really work yet */
	{"Grey", "com.obsproject.Yami.Grey"},
};

bool OBSApp::InitTheme()
{
	defaultPalette = palette();
	setStyle(new OBSProxyStyle());

	/* Set search paths for custom 'theme:' URI prefix */
	string searchDir;
	if (GetDataFilePath("themes", searchDir)) {
		auto installSearchDir = filesystem::u8path(searchDir);
		QDir::addSearchPath("theme", absolute(installSearchDir));
	}

	char userDir[512];
	if (GetConfigPath(userDir, sizeof(userDir), "obs-studio/themes")) {
		auto configSearchDir = filesystem::u8path(userDir);
		QDir::addSearchPath("theme", absolute(configSearchDir));
	}

	/* Load list of themes and read their metadata */
	FindThemes();

	/* Set up Qt file watcher to automatically reload themes */
	themeWatcher = new QFileSystemWatcher(this);
	connect(themeWatcher.get(), &QFileSystemWatcher::fileChanged, this,
		&OBSApp::themeFileChanged);

	/* Migrate old theme config key */
	if (config_has_user_value(globalConfig, "General", "CurrentTheme3") &&
	    !config_has_user_value(globalConfig, "Appearance", "Theme")) {
		const char *old = config_get_string(globalConfig, "General",
						    "CurrentTheme3");

		if (themeMigrations.count(old)) {
			config_set_string(globalConfig, "Appearance", "Theme",
					  themeMigrations[old].c_str());
		}
	}

	QString themeName =
		config_get_string(globalConfig, "Appearance", "Theme");

	if (themeName.isEmpty() || !GetTheme(themeName))
		themeName = DEFAULT_THEME;

	return SetTheme(themeName);
}
